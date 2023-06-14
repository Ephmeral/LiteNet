cmake_minimum_required(VERSION 3.10)

file(GLOB_RECURSE LITENET_TEST_SOURCES "${PROJECT_SOURCE_DIR}/test/*test.cc")
        
foreach (litenet_test_source ${LITENET_TEST_SOURCES})
    message("Build file: ${litenet_test_source}")

    # Create a human readable name.
    get_filename_component(litenet_test_filename ${litenet_test_source} NAME)
    string(REPLACE ".cc" "" litenet_test_name ${litenet_test_filename})
    
    # Add the test target separately and as part of "make check-tests".
    add_executable(${litenet_test_name} EXCLUDE_FROM_ALL ${litenet_test_source})
    
    target_link_libraries(${litenet_test_name} LiteNet)

endforeach ()
