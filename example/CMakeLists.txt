add_executable(basic_usage basic_usage.cpp)
target_link_libraries(basic_usage PRIVATE ouroboros)

add_executable(concurrent_spsc concurrent_spsc.cpp)
target_link_libraries(concurrent_spsc PRIVATE ouroboros pthread)

add_executable(mpsc_fanin mpsc_fanin.cpp)
target_link_libraries(mpsc_fanin PRIVATE ouroboros pthread)

add_executable(spmc_fanout spmc_fanout.cpp)
target_link_libraries(spmc_fanout PRIVATE ouroboros pthread)
