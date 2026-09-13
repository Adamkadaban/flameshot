find_package(Qt6 REQUIRED COMPONENTS Widgets Test)

function(add_capture_test name)
    add_executable(${name}_test ${name}_test.cpp ${ARGN})
    set_target_properties(${name}_test PROPERTIES AUTOMOC ON)
    target_include_directories(${name}_test PRIVATE ../src)
    target_link_libraries(${name}_test PRIVATE
        project_options Qt6::Widgets Qt6::Test
    )
    add_test(NAME ${name} COMMAND ${name}_test)
    set_tests_properties(${name} PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    )
endfunction()

add_capture_test(portalimage ../src/utils/portalimage.cpp)
add_capture_test(screenpointer ../src/utils/screenpointer.cpp)
add_capture_test(magnifier ../src/widgets/capture/magnifierwidget.cpp)
