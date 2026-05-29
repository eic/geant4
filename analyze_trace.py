#!/usr/bin/env python3
"""
analyze_trace.py — Geant4 perfetto trace cost analyzer.

Queries the trace using the perfetto Trace Processor SQL engine and
prints ranked cost tables by particle, detector volume, process, and
combinations thereof.  Every row is directly actionable: it names the
specific particle × process × volume triplet consuming that CPU time.

Usage:
    python analyze_trace.py  trace.pftrace  [--top N]

Requirements:
    pip install perfetto
"""

import argparse
import os
import sys


# ── Formatting helpers ────────────────────────────────────────────────────────

def fmt_time(ns):
    """Format nanoseconds as a human-readable duration string."""
    if ns is None:
        return "   N/A"
    ns = float(ns)
    if ns >= 1e9:
        return f"{ns/1e9:7.2f}s"
    if ns >= 1e6:
        return f"{ns/1e6:7.1f}ms"
    if ns >= 1e3:
        return f"{ns/1e3:7.0f}µs"
    return f"{ns:7.0f}ns"


def fmt_pct(frac):
    if frac is None:
        return "  N/A"
    return f"{float(frac)*100:5.1f}%"


def fmt_int(n):
    return f"{int(n):>10,}"


def fmt_str(s):
    return str(s) if s else "(none)"


def section(title):
    bar = "═" * 72
    print(f"\n{bar}")
    print(f"  {title}")
    print(bar)


def table(headers, rows, fmts, right_align=None):
    """
    Print a formatted table.  fmts is a list of callables, one per column.
    right_align is a set of column indices that should be right-justified.
    """
    if not rows:
        print("  (no data)")
        return
    right_align = right_align or set()
    str_rows = [[fmts[i](v) for i, v in enumerate(row)] for row in rows]
    widths = [max(len(headers[i]), max(len(r[i]) for r in str_rows))
              for i in range(len(headers))]
    sep = "  " + "  ".join("─" * w for w in widths)

    def fmt_row(cells):
        parts = []
        for i, (cell, w) in enumerate(zip(cells, widths)):
            if i in right_align:
                parts.append(cell.rjust(w))
            else:
                parts.append(cell.ljust(w))
        return "  " + "  ".join(parts)

    print(fmt_row(headers))
    print(sep)
    for r in str_rows:
        print(fmt_row(r))


def q(tp, sql):
    return list(tp.query(sql))


# ── Individual analyses ───────────────────────────────────────────────────────

def overview(tp):
    section("OVERVIEW  —  total CPU time per span category (spans are nested)")
    print("  Hierarchy: g4run > g4event > g4track > g4step > g4process/g4navigation")
    print()
    rows = q(tp, """
        SELECT category,
               COUNT(*)        AS n,
               SUM(dur)        AS total_ns,
               AVG(dur)        AS avg_ns,
               MAX(dur)        AS max_ns
        FROM   slice
        WHERE  dur > 0
        GROUP  BY category
        ORDER  BY total_ns DESC
    """)
    if not rows:
        print("  (empty trace)")
        return 1

    run_ns = next((r.total_ns for r in rows if r.category == "g4run"), None)
    denom  = float(run_ns or sum(r.total_ns for r in rows) or 1)

    table(
        ["category", "count", "total_time", "% of run", "avg/span", "max/span"],
        [(r.category, r.n, r.total_ns, r.total_ns / denom, r.avg_ns, r.max_ns)
         for r in rows],
        [fmt_str, fmt_int, fmt_time, fmt_pct, fmt_time, fmt_time],
        right_align={1, 2, 3, 4, 5},
    )
    return denom


def by_particle(tp, top_n, step_denom):
    section(f"TOP {top_n}  PARTICLES  —  by total track CPU time")
    rows = q(tp, f"""
        SELECT name,
               COUNT(*)  AS n_tracks,
               SUM(dur)  AS total_ns,
               AVG(dur)  AS avg_ns
        FROM   slice
        WHERE  category = 'g4track' AND dur > 0
        GROUP  BY name
        ORDER  BY total_ns DESC
        LIMIT  {top_n}
    """)
    denom = float(sum(r.total_ns for r in rows) or 1)
    table(
        ["particle", "n_tracks", "total_time", "% of tracks", "avg/track"],
        [(r.name, r.n_tracks, r.total_ns, r.total_ns / denom, r.avg_ns)
         for r in rows],
        [fmt_str, fmt_int, fmt_time, fmt_pct, fmt_time],
        right_align={1, 2, 3, 4},
    )


def by_volume(tp, top_n):
    section(f"TOP {top_n}  DETECTOR VOLUMES  —  by total step CPU time")
    print("  (volume = reduced subdetector label from step span name)")
    print()
    rows = q(tp, f"""
        SELECT name,
               COUNT(*)  AS n_steps,
               SUM(dur)  AS total_ns,
               AVG(dur)  AS avg_ns
        FROM   slice
        WHERE  category = 'g4step' AND dur > 0
        GROUP  BY name
        ORDER  BY total_ns DESC
        LIMIT  {top_n}
    """)
    denom = float(sum(r.total_ns for r in rows) or 1)
    table(
        ["volume", "n_steps", "total_time", "% of steps", "avg/step"],
        [(r.name, r.n_steps, r.total_ns, r.total_ns / denom, r.avg_ns)
         for r in rows],
        [fmt_str, fmt_int, fmt_time, fmt_pct, fmt_time],
        right_align={1, 2, 3, 4},
    )
    return denom


def by_process(tp, top_n):
    section(f"TOP {top_n}  PROCESSES  —  by total CPU time")
    rows = q(tp, f"""
        SELECT name,
               COUNT(*)  AS n_calls,
               SUM(dur)  AS total_ns,
               AVG(dur)  AS avg_ns
        FROM   slice
        WHERE  category = 'g4process' AND dur > 0
        GROUP  BY name
        ORDER  BY total_ns DESC
        LIMIT  {top_n}
    """)
    if not rows:
        print("  (no data — g4process requires /profiling/verbose 3)")
        return 1.0
    denom = float(sum(r.total_ns for r in rows) or 1)
    table(
        ["process", "n_calls", "total_time", "% of procs", "avg/call"],
        [(r.name, r.n_calls, r.total_ns, r.total_ns / denom, r.avg_ns)
         for r in rows],
        [fmt_str, fmt_int, fmt_time, fmt_pct, fmt_time],
        right_align={1, 2, 3, 4},
    )
    return denom


def step_breakdown(tp):
    section("STEP TIME BREAKDOWN  —  processes vs navigation vs overhead")
    row = q(tp, """
        WITH
          s AS (SELECT SUM(dur) AS t FROM slice WHERE category='g4step'       AND dur>0),
          p AS (SELECT SUM(dur) AS t FROM slice WHERE category='g4process'    AND dur>0),
          n AS (SELECT SUM(dur) AS t FROM slice WHERE category='g4navigation' AND dur>0)
        SELECT s.t AS step_ns,
               p.t AS proc_ns,
               n.t AS nav_ns
        FROM s, p, n
    """)
    if not row:
        print("  (no step data)")
        return
    r = row[0]
    step_ns     = float(r.step_ns or 0)
    proc_ns     = float(r.proc_ns or 0)
    nav_ns      = float(r.nav_ns  or 0)
    overhead_ns = step_ns - proc_ns - nav_ns
    denom       = step_ns or 1.0

    def pct(v): return f"({v/denom*100:.1f}%)"
    def note(v, label): return f"  [{label} not recorded — use /profiling/verbose 3]" if v == 0 else ""

    print(f"  Total step time :  {fmt_time(step_ns).strip()}")
    print(f"  ├─ processes    :  {fmt_time(proc_ns).strip()}  {pct(proc_ns)}"
          f"{note(proc_ns, 'g4process')}")
    print(f"  ├─ navigation   :  {fmt_time(nav_ns).strip()}  {pct(nav_ns)}"
          f"{note(nav_ns, 'g4navigation')}")
    print(f"  └─ overhead     :  {fmt_time(overhead_ns).strip()}  {pct(overhead_ns)}"
          f"  (physics lookups, safety, track update, etc.)")


def by_process_x_volume(tp, top_n, proc_denom):
    section(f"TOP {top_n}  PROCESS × VOLUME  —  most expensive combinations")
    rows = q(tp, f"""
        SELECT p.name   AS process,
               s.name   AS volume,
               COUNT(*) AS n_calls,
               SUM(p.dur) AS total_ns,
               AVG(p.dur) AS avg_ns
        FROM   slice p
        JOIN   slice s ON s.id = p.parent_id
        WHERE  p.category = 'g4process'
          AND  s.category = 'g4step'
          AND  p.dur > 0
        GROUP  BY p.name, s.name
        ORDER  BY total_ns DESC
        LIMIT  {top_n}
    """)
    if not rows:
        print("  (no data — g4process requires /profiling/verbose 3)")
        return
    table(
        ["process", "volume", "n_calls", "total_time", "% of procs", "avg/call"],
        [(r.process, r.volume, r.n_calls, r.total_ns,
          r.total_ns / proc_denom, r.avg_ns) for r in rows],
        [fmt_str, fmt_str, fmt_int, fmt_time, fmt_pct, fmt_time],
        right_align={2, 3, 4, 5},
    )


def by_particle_x_process(tp, top_n, proc_denom):
    section(f"TOP {top_n}  PARTICLE × PROCESS  —  most expensive combinations")
    rows = q(tp, f"""
        SELECT t.name   AS particle,
               p.name   AS process,
               COUNT(*) AS n_calls,
               SUM(p.dur) AS total_ns,
               AVG(p.dur) AS avg_ns
        FROM   slice p
        JOIN   slice s ON s.id = p.parent_id
        JOIN   slice t ON t.id = s.parent_id
        WHERE  p.category = 'g4process'
          AND  s.category = 'g4step'
          AND  t.category = 'g4track'
          AND  p.dur > 0
        GROUP  BY t.name, p.name
        ORDER  BY total_ns DESC
        LIMIT  {top_n}
    """)
    if not rows:
        print("  (no data — g4process requires /profiling/verbose 3)")
        return
    table(
        ["particle", "process", "n_calls", "total_time", "% of procs", "avg/call"],
        [(r.particle, r.process, r.n_calls, r.total_ns,
          r.total_ns / proc_denom, r.avg_ns) for r in rows],
        [fmt_str, fmt_str, fmt_int, fmt_time, fmt_pct, fmt_time],
        right_align={2, 3, 4, 5},
    )


def by_particle_x_volume(tp, top_n, step_denom):
    section(f"TOP {top_n}  PARTICLE × VOLUME  —  most expensive combinations (step time)")
    rows = q(tp, f"""
        SELECT t.name    AS particle,
               s.name    AS volume,
               COUNT(*)  AS n_steps,
               SUM(s.dur) AS total_ns
        FROM   slice s
        JOIN   slice t ON t.id = s.parent_id
        WHERE  s.category = 'g4step'
          AND  t.category = 'g4track'
          AND  s.dur > 0
        GROUP  BY t.name, s.name
        ORDER  BY total_ns DESC
        LIMIT  {top_n}
    """)
    if not rows:
        print("  (no data — g4step requires /profiling/verbose 2)")
        return
    table(
        ["particle", "volume", "n_steps", "total_time", "% of steps"],
        [(r.particle, r.volume, r.n_steps, r.total_ns,
          r.total_ns / step_denom) for r in rows],
        [fmt_str, fmt_str, fmt_int, fmt_time, fmt_pct],
        right_align={2, 3, 4},
    )


def by_particle_x_process_x_volume(tp, top_n, proc_denom):
    section(f"TOP {top_n}  PARTICLE × PROCESS × VOLUME  —  actionable triplets")
    rows = q(tp, f"""
        SELECT t.name   AS particle,
               p.name   AS process,
               s.name   AS volume,
               COUNT(*) AS n_calls,
               SUM(p.dur) AS total_ns
        FROM   slice p
        JOIN   slice s ON s.id = p.parent_id
        JOIN   slice t ON t.id = s.parent_id
        WHERE  p.category = 'g4process'
          AND  s.category = 'g4step'
          AND  t.category = 'g4track'
          AND  p.dur > 0
        GROUP  BY t.name, p.name, s.name
        ORDER  BY total_ns DESC
        LIMIT  {top_n}
    """)
    if not rows:
        print("  (no data — g4process requires /profiling/verbose 3)")
        return
    table(
        ["particle", "process", "volume", "n_calls", "total_time", "% of procs"],
        [(r.particle, r.process, r.volume, r.n_calls, r.total_ns,
          r.total_ns / proc_denom) for r in rows],
        [fmt_str, fmt_str, fmt_str, fmt_int, fmt_time, fmt_pct],
        right_align={3, 4, 5},
    )


def navigation_by_pv(tp, top_n):
    section(f"TOP {top_n}  NAVIGATION cost by detector volume (from parent g4step)")
    rows = q(tp, f"""
        SELECT p.name          AS volume,
               COUNT(*)        AS n_calls,
               SUM(n.dur)      AS total_ns,
               AVG(n.dur)      AS avg_ns
        FROM   slice n
        JOIN   slice p ON p.id = n.parent_id
        WHERE  n.category = 'g4navigation'
          AND  p.category = 'g4step'
          AND  n.dur > 0
        GROUP  BY p.name
        ORDER  BY total_ns DESC
        LIMIT  {top_n}
    """)
    if not rows:
        print("  (no data — g4navigation requires /profiling/verbose 3)")
        return
    denom = float(sum(r.total_ns for r in rows) or 1)
    table(
        ["detector_volume", "n_calls", "total_time", "% of nav", "avg/call"],
        [(r.volume, r.n_calls, r.total_ns, r.total_ns / denom, r.avg_ns)
         for r in rows],
        [fmt_str, fmt_int, fmt_time, fmt_pct, fmt_time],
        right_align={1, 2, 3, 4},
    )


def secondary_flow_stats(tp):
    """Show which processes produce the most secondaries (from flow events)."""
    section("SECONDARY PRODUCTION  —  flow event count by process × particle")
    print("  (counts 'secondary' instant events emitted inside g4process spans)")
    print()
    rows = q(tp, """
        SELECT p.name  AS process,
               COUNT(*) AS n_secondaries
        FROM   slice p
        JOIN   slice src ON src.parent_id = p.id
        WHERE  p.category   = 'g4process'
          AND  src.category = 'g4process'
          AND  src.name     = 'secondary'
        GROUP  BY p.name
        ORDER  BY n_secondaries DESC
        LIMIT  20
    """)
    if not rows:
        # flows are instant events, not child slices — try args approach
        rows = q(tp, """
            SELECT name AS process, COUNT(*) AS n_secondaries
            FROM   slice
            WHERE  category = 'g4process' AND name = 'secondary'
            LIMIT  1
        """)
        print("  (no secondary flow events found — check perfetto version)")
        return
    table(
        ["process", "n_secondaries"],
        [(r.process, r.n_secondaries) for r in rows],
        [fmt_str, fmt_int],
        right_align={1},
    )


# ── Main ─────────────────────────────────────────────────────────────────────

def main():
    parser = argparse.ArgumentParser(
        description="Analyze Geant4 perfetto trace for computational cost sinks.",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=__doc__,
    )
    parser.add_argument("trace", help="Path to .pftrace file")
    parser.add_argument(
        "--top", type=int, default=15,
        help="Rows per table (default: 15)",
    )
    parser.add_argument(
        "--sections", nargs="*",
        help="Run only specific sections (overview particle volume process "
             "step_breakdown process_x_volume particle_x_process "
             "particle_x_volume triplets navigation secondaries)",
    )
    args = parser.parse_args()

    if not os.path.exists(args.trace):
        sys.exit(f"Error: trace file not found: {args.trace}")

    size_mb = os.path.getsize(args.trace) / 1e6
    print(f"Loading trace: {args.trace}  ({size_mb:.0f} MB)")

    try:
        from perfetto.trace_processor import TraceProcessor
    except ImportError:
        sys.exit("Error: perfetto package not found.  Run: pip install perfetto")

    tp = TraceProcessor(trace=args.trace)

    want = set(args.sections) if args.sections else None

    def wanted(name):
        return want is None or name in want

    run_ns    = 1.0
    step_ns   = 1.0
    proc_ns   = 1.0

    if wanted("overview"):
        run_ns = overview(tp) or 1.0

    if wanted("particle"):
        by_particle(tp, args.top, run_ns)

    if wanted("volume"):
        step_ns = by_volume(tp, args.top) or 1.0

    if wanted("process"):
        proc_ns = by_process(tp, args.top) or 1.0

    if wanted("step_breakdown"):
        step_breakdown(tp)

    # For cross-table denominators, recompute if sections were skipped
    if proc_ns == 1.0:
        r = q(tp, "SELECT SUM(dur) AS t FROM slice WHERE category='g4process' AND dur>0")
        proc_ns = float(r[0].t or 1) if r else 1.0
    if step_ns == 1.0:
        r = q(tp, "SELECT SUM(dur) AS t FROM slice WHERE category='g4step' AND dur>0")
        step_ns = float(r[0].t or 1) if r else 1.0

    if wanted("process_x_volume"):
        by_process_x_volume(tp, args.top, proc_ns)

    if wanted("particle_x_process"):
        by_particle_x_process(tp, args.top, proc_ns)

    if wanted("particle_x_volume"):
        by_particle_x_volume(tp, args.top, step_ns)

    if wanted("triplets"):
        by_particle_x_process_x_volume(tp, args.top, proc_ns)

    if wanted("navigation"):
        navigation_by_pv(tp, args.top)

    if wanted("secondaries"):
        secondary_flow_stats(tp)

    print()


if __name__ == "__main__":
    main()
