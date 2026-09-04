cmake_minimum_required(VERSION 3.16)

project(zhu_screen_pet VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

option(ZHU_SCREEN_PET_BUILD_TESTS "Build zhu_screen_pet smoke tests" ON)
find_package(Qt5 5.15 REQUIRED COMPONENTS Core Gui Widgets Network Sql)

set(ZHU_SCREEN_PET_OPENSSL_RUNTIME_DIR "" CACHE PATH
    "Directory containing the OpenSSL runtime DLLs used by Qt")
set(ZHU_SCREEN_PET_MSVC_RUNTIME_DIR "" CACHE PATH
    "Directory containing the redistributable MSVC x64 CRT DLLs")
if (MSVC AND NOT ZHU_SCREEN_PET_MSVC_RUNTIME_DIR
    AND DEFINED ENV{VCToolsRedistDir})
    file(TO_CMAKE_PATH "$ENV{VCToolsRedistDir}" ZHU_SCREEN_PET_MSVC_REDIST_ROOT)
    file(GLOB ZHU_SCREEN_PET_MSVC_RUNTIME_CANDIDATES LIST_DIRECTORIES true
        "${ZHU_SCREEN_PET_MSVC_REDIST_ROOT}/x64/Microsoft.VC*.CRT")
    list(LENGTH ZHU_SCREEN_PET_MSVC_RUNTIME_CANDIDATES
        ZHU_SCREEN_PET_MSVC_RUNTIME_CANDIDATE_COUNT)
    if (ZHU_SCREEN_PET_MSVC_RUNTIME_CANDIDATE_COUNT GREATER 0)
        list(GET ZHU_SCREEN_PET_MSVC_RUNTIME_CANDIDATES 0
            ZHU_SCREEN_PET_MSVC_RUNTIME_CANDIDATE)
        set(ZHU_SCREEN_PET_MSVC_RUNTIME_DIR
            "${ZHU_SCREEN_PET_MSVC_RUNTIME_CANDIDATE}" CACHE PATH
            "Directory containing the redistributable MSVC x64 CRT DLLs" FORCE)
    endif()
endif()
if (WIN32 AND (NOT ZHU_SCREEN_PET_OPENSSL_RUNTIME_DIR
    OR ZHU_SCREEN_PET_OPENSSL_RUNTIME_DIR MATCHES
        "/Tools/OpenSSLv3/Win_x64/bin$"))
    get_filename_component(ZHU_SCREEN_PET_QT_PREFIX
        "${Qt5_DIR}/../../.." ABSOLUTE)
    get_filename_component(ZHU_SCREEN_PET_QT_ROOT
        "${ZHU_SCREEN_PET_QT_PREFIX}/../.." ABSOLUTE)
    set(ZHU_SCREEN_PET_OPENSSL_CANDIDATE
        "${ZHU_SCREEN_PET_QT_ROOT}/Tools/OpenSSL_1_1_1w/runtime")
    if (EXISTS "${ZHU_SCREEN_PET_OPENSSL_CANDIDATE}/libssl-1_1-x64.dll"
        AND EXISTS "${ZHU_SCREEN_PET_OPENSSL_CANDIDATE}/libcrypto-1_1-x64.dll")
        set(ZHU_SCREEN_PET_OPENSSL_RUNTIME_DIR
            "${ZHU_SCREEN_PET_OPENSSL_CANDIDATE}" CACHE PATH
            "Directory containing the OpenSSL runtime DLLs used by Qt" FORCE)
    endif()
endif()

configure_file("${CMAKE_CURRENT_SOURCE_DIR}/config/model-providers.json"
               "${CMAKE_CURRENT_BINARY_DIR}/config/model-providers.json" COPYONLY)
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/config/app-settings.json"
               "${CMAKE_CURRENT_BINARY_DIR}/config/app-settings.json" COPYONLY)

# 保留源码自发现，但按目标分别发现；新增文件会重新配置，层间依赖由链接方向约束。
file(GLOB_RECURSE ZHU_SCREEN_PET_CORE_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/core/*.cpp" "${CMAKE_CURRENT_SOURCE_DIR}/src/core/*.h")
file(GLOB_RECURSE ZHU_SCREEN_PET_INFRA_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/infrastructure/*.cpp" "${CMAKE_CURRENT_SOURCE_DIR}/src/infrastructure/*.h")
file(GLOB_RECURSE ZHU_SCREEN_PET_MODEL_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/model/*.cpp" "${CMAKE_CURRENT_SOURCE_DIR}/src/model/*.h")
file(GLOB_RECURSE ZHU_SCREEN_PET_MEMORY_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/memory/*.cpp" "${CMAKE_CURRENT_SOURCE_DIR}/src/memory/*.h")
file(GLOB_RECURSE ZHU_SCREEN_PET_APP_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/app/*.cpp" "${CMAKE_CURRENT_SOURCE_DIR}/src/app/*.h")
list(FILTER ZHU_SCREEN_PET_APP_SOURCES EXCLUDE REGEX "ApplicationBootstrapper\\.(cpp|h)$")
file(GLOB_RECURSE ZHU_SCREEN_PET_UI_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/ui/*.cpp" "${CMAKE_CURRENT_SOURCE_DIR}/src/ui/*.h")
file(GLOB_RECURSE ZHU_SCREEN_PET_RESOURCE_FILES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/resources/*.qrc")

function(zhu_screen_pet_configure_target target)
    target_include_directories(${target} PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")
    if (MSVC)
        target_compile_options(${target} PRIVATE /W4 /permissive- /utf-8)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endfunction()

function(zhu_screen_pet_deploy_openssl target)
    if (NOT WIN32)
        return()
    endif()
    set(runtime_files
        "${ZHU_SCREEN_PET_OPENSSL_RUNTIME_DIR}/libssl-1_1-x64.dll"
        "${ZHU_SCREEN_PET_OPENSSL_RUNTIME_DIR}/libcrypto-1_1-x64.dll")
    foreach(runtime_file IN LISTS runtime_files)
        if (NOT EXISTS "${runtime_file}")
            message(WARNING
                "OpenSSL runtime DLL was not found: ${runtime_file}. "
                "HTTPS will be unavailable for ${target}.")
            return()
        endif()
    endforeach()
    add_custom_command(TARGET ${target} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${runtime_files} "$<TARGET_FILE_DIR:${target}>"
        COMMENT "Deploying the OpenSSL runtime for ${target}"
        VERBATIM)
endfunction()

add_library(zhu_screen_pet_core STATIC ${ZHU_SCREEN_PET_CORE_SOURCES})
zhu_screen_pet_configure_target(zhu_screen_pet_core)
target_link_libraries(zhu_screen_pet_core PUBLIC Qt5::Core)

add_library(zhu_screen_pet_infrastructure STATIC ${ZHU_SCREEN_PET_INFRA_SOURCES})
zhu_screen_pet_configure_target(zhu_screen_pet_infrastructure)
target_link_libraries(zhu_screen_pet_infrastructure PUBLIC zhu_screen_pet_core
    Qt5::Core Qt5::Gui Qt5::Widgets Qt5::Network Qt5::Sql)
if (WIN32)
    target_link_libraries(zhu_screen_pet_infrastructure PUBLIC Advapi32)
endif()

add_library(zhu_screen_pet_model STATIC ${ZHU_SCREEN_PET_MODEL_SOURCES})
zhu_screen_pet_configure_target(zhu_screen_pet_model)
target_link_libraries(zhu_screen_pet_model PUBLIC zhu_screen_pet_core zhu_screen_pet_infrastructure Qt5::Core Qt5::Network)

add_library(zhu_screen_pet_memory STATIC ${ZHU_SCREEN_PET_MEMORY_SOURCES})
zhu_screen_pet_configure_target(zhu_screen_pet_memory)
target_link_libraries(zhu_screen_pet_memory PUBLIC zhu_screen_pet_core zhu_screen_pet_model zhu_screen_pet_infrastructure Qt5::Core Qt5::Sql)

add_library(zhu_screen_pet_app STATIC ${ZHU_SCREEN_PET_APP_SOURCES})
zhu_screen_pet_configure_target(zhu_screen_pet_app)
target_link_libraries(zhu_screen_pet_app PUBLIC zhu_screen_pet_core zhu_screen_pet_model zhu_screen_pet_memory
    zhu_screen_pet_infrastructure Qt5::Core)

add_library(zhu_screen_pet_ui STATIC ${ZHU_SCREEN_PET_UI_SOURCES})
zhu_screen_pet_configure_target(zhu_screen_pet_ui)
target_link_libraries(zhu_screen_pet_ui PUBLIC zhu_screen_pet_core zhu_screen_pet_app zhu_screen_pet_memory zhu_screen_pet_model
    Qt5::Core Qt5::Gui Qt5::Widgets)

add_executable(zhu_screen_pet WIN32
    "${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/app/ApplicationBootstrapper.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/app/ApplicationBootstrapper.h"
    ${ZHU_SCREEN_PET_RESOURCE_FILES})
zhu_screen_pet_configure_target(zhu_screen_pet)
zhu_screen_pet_deploy_openssl(zhu_screen_pet)
target_link_libraries(zhu_screen_pet PRIVATE zhu_screen_pet_ui zhu_screen_pet_app zhu_screen_pet_memory zhu_screen_pet_model
    zhu_screen_pet_infrastructure zhu_screen_pet_core Qt5::Widgets)
set_target_properties(zhu_screen_pet PROPERTIES VERSION ${PROJECT_VERSION} OUTPUT_NAME "小珠看着你")

# 生成可复制到无 Qt 开发环境电脑上运行的 Windows 便携发布目录。
# 该目标独立于普通编译，避免每次开发构建都执行 windeployqt。
if (WIN32)
    get_filename_component(ZHU_SCREEN_PET_QT_BIN_DIR
        "${Qt5_DIR}/../../../bin" ABSOLUTE)
    find_program(ZHU_SCREEN_PET_WINDEPLOYQT_EXECUTABLE
        NAMES windeployqt
        HINTS "${ZHU_SCREEN_PET_QT_BIN_DIR}"
        NO_DEFAULT_PATH)
    if (ZHU_SCREEN_PET_WINDEPLOYQT_EXECUTABLE
        AND EXISTS "${ZHU_SCREEN_PET_MSVC_RUNTIME_DIR}/msvcp140.dll"
        AND EXISTS "${ZHU_SCREEN_PET_MSVC_RUNTIME_DIR}/vcruntime140.dll")
        set(ZHU_SCREEN_PET_PORTABLE_DIR
            "${CMAKE_BINARY_DIR}/dist/$<CONFIG>")
        add_custom_target(zhu_screen_pet_portable
            COMMAND ${CMAKE_COMMAND} -E remove_directory
                "${ZHU_SCREEN_PET_PORTABLE_DIR}"
            COMMAND ${CMAKE_COMMAND} -E make_directory
                "${ZHU_SCREEN_PET_PORTABLE_DIR}"
            COMMAND ${CMAKE_COMMAND} -E copy
                "$<TARGET_FILE:zhu_screen_pet>"
                "${ZHU_SCREEN_PET_PORTABLE_DIR}/$<TARGET_FILE_NAME:zhu_screen_pet>"
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${CMAKE_BINARY_DIR}/config"
                "${ZHU_SCREEN_PET_PORTABLE_DIR}/config"
            COMMAND "${ZHU_SCREEN_PET_WINDEPLOYQT_EXECUTABLE}"
                "$<IF:$<CONFIG:Debug>,--debug,--release>"
                --no-compiler-runtime
                --no-translations
                --force
                --dir "${ZHU_SCREEN_PET_PORTABLE_DIR}"
                "${ZHU_SCREEN_PET_PORTABLE_DIR}/$<TARGET_FILE_NAME:zhu_screen_pet>"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${ZHU_SCREEN_PET_OPENSSL_RUNTIME_DIR}/libssl-1_1-x64.dll"
                "${ZHU_SCREEN_PET_OPENSSL_RUNTIME_DIR}/libcrypto-1_1-x64.dll"
                "${ZHU_SCREEN_PET_PORTABLE_DIR}"
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${ZHU_SCREEN_PET_MSVC_RUNTIME_DIR}"
                "${ZHU_SCREEN_PET_PORTABLE_DIR}"
            DEPENDS zhu_screen_pet
            COMMENT "Creating portable zhu_screen_pet package"
            VERBATIM
            USES_TERMINAL)
    elseif (NOT ZHU_SCREEN_PET_WINDEPLOYQT_EXECUTABLE)
        message(WARNING
            "windeployqt was not found under ${ZHU_SCREEN_PET_QT_BIN_DIR}; "
            "the zhu_screen_pet_portable target will not be available.")
    else()
        message(WARNING
            "Redistributable MSVC x64 CRT DLLs were not found. Configure "
            "ZHU_SCREEN_PET_MSVC_RUNTIME_DIR to enable the "
            "zhu_screen_pet_portable target.")
    endif()
endif()

include(CTest)
if (ZHU_SCREEN_PET_BUILD_TESTS)
    enable_testing()
    find_package(Qt5 5.15 REQUIRED COMPONENTS Test)
    get_filename_component(ZHU_SCREEN_PET_QT_PREFIX "${Qt5_DIR}/../../.." ABSOLUTE)
    function(zhu_screen_pet_add_test target source)
        add_executable(${target} "${source}")
        zhu_screen_pet_configure_target(${target})
        zhu_screen_pet_deploy_openssl(${target})
        target_link_libraries(${target} PRIVATE zhu_screen_pet_ui zhu_screen_pet_app zhu_screen_pet_memory
            zhu_screen_pet_model zhu_screen_pet_infrastructure zhu_screen_pet_core Qt5::Test Qt5::Widgets)
        add_test(NAME ${target} COMMAND ${target})
        set_tests_properties(${target} PROPERTIES
            ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_PLUGIN_PATH=${ZHU_SCREEN_PET_QT_PREFIX}/plugins")
    endfunction()
    zhu_screen_pet_add_test(zhu_screen_pet_smoke_test
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/smoke_test.cpp")
    zhu_screen_pet_add_test(zhu_screen_pet_model_provider_test
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/model_provider_test.cpp")
    zhu_screen_pet_add_test(zhu_screen_pet_memory_chat_test
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/memory_chat_test.cpp")
    zhu_screen_pet_add_test(zhu_screen_pet_legacy_migration_test
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/legacy_migration_test.cpp")
endif()
