add_executable(EventProcessing.Console
    EventProcessing.Console.cpp
)

target_link_libraries(EventProcessing.Console PRIVATE EventProcessing::Core)

set_target_properties(EventProcessing.Console PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)

eventcore_copy_windows_runtime_deps(EventProcessing.Console)
