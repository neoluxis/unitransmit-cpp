cmake_minimum_required(VERSION 3.16)

project(unitransmit)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(UNITRANSMIT_BUILD_PYTHON "Build Python bindings" OFF)

set(SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/src/UniTransmit.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/Protocol.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/Serial.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/UDP.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/TCP.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/GATT.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/MQTT.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/Utils.cpp
)

add_library(${PROJECT_NAME}_static STATIC ${SOURCES})
add_library(${PROJECT_NAME}_shared SHARED ${SOURCES})

set_target_properties(${PROJECT_NAME}_static PROPERTIES POSITION_INDEPENDENT_CODE ON)

target_include_directories(${PROJECT_NAME}_static PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_include_directories(${PROJECT_NAME}_shared PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

set_target_properties(${PROJECT_NAME}_static PROPERTIES OUTPUT_NAME ${PROJECT_NAME})
set_target_properties(${PROJECT_NAME}_shared PROPERTIES OUTPUT_NAME ${PROJECT_NAME})

if (WIN32)
    target_link_libraries(${PROJECT_NAME}_static PUBLIC ws2_32)
    target_link_libraries(${PROJECT_NAME}_shared PUBLIC ws2_32)
endif ()

if (UNITRANSMIT_BUILD_PYTHON)
    if (SKBUILD)
        # pip / scikit-build-core 构建：
        # 使用构建后端提供的 Python 环境。
        set(PYBIND11_FINDPYTHON ON)
    else ()
        # 直接使用 CMake：要求显式指定 Python。
        if (NOT Python_EXECUTABLE)
            message(FATAL_ERROR
                "Specify Python with -DPython_EXECUTABLE=/path/to/python"
            )
        endif ()
        set(PYBIND11_FINDPYTHON ON)
    endif ()

    find_package(Python REQUIRED COMPONENTS Interpreter Development.Module)

    # 从选定的 Python 环境定位 pybind11。
    execute_process(
        COMMAND "${Python_EXECUTABLE}" -m pybind11 --cmakedir
        OUTPUT_VARIABLE pybind11_DIR
        OUTPUT_STRIP_TRAILING_WHITESPACE
        RESULT_VARIABLE PYBIND11_QUERY_RESULT
    )
    if (NOT PYBIND11_QUERY_RESULT EQUAL 0)
        message(FATAL_ERROR
            "pybind11 is unavailable in ${Python_EXECUTABLE}"
        )
    endif ()

    find_package(pybind11 CONFIG REQUIRED)

    pybind11_add_module(_unitransmit python/bindings.cpp)
    target_link_libraries(_unitransmit PRIVATE ${PROJECT_NAME}_static)

    install(TARGETS _unitransmit
        LIBRARY DESTINATION unitransmit
        RUNTIME DESTINATION unitransmit
    )
endif ()

enable_testing()
add_executable(unitransmit_tests_core
        ${CMAKE_CURRENT_SOURCE_DIR}/test/core_test.cpp
)
target_link_libraries(unitransmit_tests_core PRIVATE ${PROJECT_NAME}_static)
target_include_directories(unitransmit_tests_core PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)
add_test(NAME unitransmit_tests_core COMMAND unitransmit_tests_core)

add_executable(unitransmit_tests_tcp
        ${CMAKE_CURRENT_SOURCE_DIR}/test/tcp_test.cpp
)
target_link_libraries(unitransmit_tests_tcp PRIVATE ${PROJECT_NAME}_static)
target_include_directories(unitransmit_tests_tcp PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)
add_test(NAME unitransmit_tests_tcp COMMAND unitransmit_tests_tcp)

add_executable(unitransmit_tests_udp
        ${CMAKE_CURRENT_SOURCE_DIR}/test/udp_test.cpp
)
target_link_libraries(unitransmit_tests_udp PRIVATE ${PROJECT_NAME}_static)
target_include_directories(unitransmit_tests_udp PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)
add_test(NAME unitransmit_tests_udp COMMAND unitransmit_tests_udp)

add_executable(unitransmit_tests_serial
        ${CMAKE_CURRENT_SOURCE_DIR}/test/serial_test.cpp
)
target_link_libraries(unitransmit_tests_serial PRIVATE ${PROJECT_NAME}_static)
target_include_directories(unitransmit_tests_serial PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)
add_test(NAME unitransmit_tests_serial COMMAND unitransmit_tests_serial)

add_executable(unitransmit_tests_gatt
        ${CMAKE_CURRENT_SOURCE_DIR}/test/gatt_test.cpp
)
target_link_libraries(unitransmit_tests_gatt PRIVATE ${PROJECT_NAME}_static)
target_include_directories(unitransmit_tests_gatt PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)
add_test(NAME unitransmit_tests_gatt COMMAND unitransmit_tests_gatt)

add_executable(unitransmit_tests_mqtt
        ${CMAKE_CURRENT_SOURCE_DIR}/test/mqtt_test.cpp
)
target_link_libraries(unitransmit_tests_mqtt PRIVATE ${PROJECT_NAME}_static)
target_include_directories(unitransmit_tests_mqtt PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)
add_test(NAME unitransmit_tests_mqtt COMMAND unitransmit_tests_mqtt)

add_executable(unitransmit_terminal_sim
        ${CMAKE_CURRENT_SOURCE_DIR}/test/terminal_sim.cpp
)
target_link_libraries(unitransmit_terminal_sim PRIVATE ${PROJECT_NAME}_static)
target_include_directories(unitransmit_terminal_sim PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

add_executable(unitransmit_demo_loopback
        ${CMAKE_CURRENT_SOURCE_DIR}/demo/cpp/loopback.cpp
)
target_link_libraries(unitransmit_demo_loopback PRIVATE ${PROJECT_NAME}_static)
target_include_directories(unitransmit_demo_loopback PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

add_executable(unitransmit_demo_udp_pair
        ${CMAKE_CURRENT_SOURCE_DIR}/demo/cpp/udp_pair.cpp
)
target_link_libraries(unitransmit_demo_udp_pair PRIVATE ${PROJECT_NAME}_static)
target_include_directories(unitransmit_demo_udp_pair PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

add_executable(unitransmit_demo_tcp_pair
        ${CMAKE_CURRENT_SOURCE_DIR}/demo/cpp/tcp_pair.cpp
)
target_link_libraries(unitransmit_demo_tcp_pair PRIVATE ${PROJECT_NAME}_static)
target_include_directories(unitransmit_demo_tcp_pair PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)
