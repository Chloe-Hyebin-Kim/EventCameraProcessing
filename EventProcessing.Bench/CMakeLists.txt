add_executable(EventProcessing.Bench
    EventProcessing.Bench.cpp
)

target_link_libraries(EventProcessing.Bench PRIVATE EventProcessing::Core)

set_target_properties(EventProcessing.Bench PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)

eventcore_copy_windows_runtime_deps(EventProcessing.Bench)
