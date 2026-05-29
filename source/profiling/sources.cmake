# - G4profiling module build definition

configure_file(${CMAKE_CURRENT_LIST_DIR}/include/G4Profiling/G4ProfilingConfig.hh.in
  ${CMAKE_CURRENT_BINARY_DIR}/include/G4Profiling/G4ProfilingConfig.hh)

geant4_add_module(G4profiling
  PUBLIC_HEADERS
    ${CMAKE_CURRENT_BINARY_DIR}/include/G4Profiling/G4ProfilingConfig.hh
    G4Profiling/G4ScopedProfiling.hh
    G4Profiling/G4TracingSession.hh
    G4Profiling/G4ProfilingManager.hh
    G4Profiling/G4ProfilingMessenger.hh
  PRIVATE_HEADERS
    ${CMAKE_CURRENT_LIST_DIR}/src/detail/G4ProfilingCategories.perfetto.hh
  SOURCES
    G4ProfilingManager.cc
    G4ProfilingMessenger.cc)

geant4_module_include_directories(G4profiling
  PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include>)

if(GEANT4_USE_PERFETTO)
  geant4_module_sources(G4profiling
    SOURCES
      G4ScopedProfiling.cc
      G4TracingSession.perfetto.cc)
  geant4_module_link_libraries(G4profiling
    PUBLIC G4intercoms G4globman
    PRIVATE G4ExtPerfetto)
else()
  geant4_module_sources(G4profiling
    SOURCES G4ScopedProfiling.stub.cc)
  geant4_module_link_libraries(G4profiling
    PUBLIC G4intercoms G4globman)
endif()

geant4_module_compile_definitions(G4profiling
  PUBLIC
    $<$<BOOL:${GEANT4_USE_PROFILING}>:GEANT4_USE_PROFILING>
    $<$<BOOL:${GEANT4_USE_PERFETTO}>:GEANT4_USE_PERFETTO>)

set_property(GLOBAL APPEND
  PROPERTY GEANT4_BUILDTREE_INCLUDE_DIRS "${CMAKE_CURRENT_BINARY_DIR}/include")
