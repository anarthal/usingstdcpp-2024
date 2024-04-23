
cmake_minimum_required(VERSION 3.15.0)
project(usingstdcpp_2024)

find_package(Boost REQUIRED)

function(add_example EXE)
    add_executable(${EXE} ${EXE}.cpp)
    target_link_libraries(${EXE} PRIVATE Boost::headers)
    target_compile_features(${EXE} PRIVATE cxx_std_20)
endfunction()

add_example(sync)
add_example(callbacks)
add_example(coroutines)
add_example(as_tuple)
add_example(associated_allocator)
add_example(cancellation)
add_example(cancellation_composed)
add_example(timeouts)
add_example(beast)
add_example(composed)