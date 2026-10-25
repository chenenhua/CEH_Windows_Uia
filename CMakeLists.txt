cmake_minimum_required(VERSION 3.16)

project(CEH_Windows_Uia LANGUAGES C CXX)

set(APP_NAME "CEH_Windows_Uia")
set(APP_OUTPUT_NAME "木头自动系统控制")
set(APP_VERSION "1.0.0")

set(PROJECT_ROOT "${CMAKE_CURRENT_SOURCE_DIR}")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
    add_compile_options(
            /MP
            /utf-8
            /permissive-
            /W4
    )
endif()

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

set(QT_DIR_PATH "C:/Qt/6.11.1/msvc2022_64")
list(APPEND CMAKE_PREFIX_PATH "${QT_DIR_PATH}")

set(OpenCV_DIR "C:/opencv/opencv/build/x64/vc16/lib")
set(OPENCV_DLL_BASE_DIR "C:/opencv/opencv/build/x64/vc16/bin")

find_package(
        Qt6
        REQUIRED
        COMPONENTS
        Core
        Gui
        Widgets
        Network
        Sql
        Concurrent
)

find_package(OpenCV REQUIRED)

set(
        APP_SOURCES
        main.cpp
        resources.qrc
        platform/Windows/app.rc
)

qt_add_executable(
        ${APP_NAME}
        WIN32
        ${APP_SOURCES}
)

target_compile_definitions(
        ${APP_NAME}
        PRIVATE
        CEH_APP_NAME="${APP_OUTPUT_NAME}"
        CEH_APP_VERSION="${APP_VERSION}"
        CEH_PROJECT_ROOT="${PROJECT_ROOT}"
        NOMINMAX
        WIN32_LEAN_AND_MEAN
        UNICODE
        _UNICODE
)

target_include_directories(
        ${APP_NAME}
        PRIVATE
        ${PROJECT_ROOT}
        ${OpenCV_INCLUDE_DIRS}
)

target_link_libraries(
        ${APP_NAME}
        PRIVATE

        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
        Qt6::Network
        Qt6::Sql
        Qt6::Concurrent

        ${OpenCV_LIBS}

        user32
        gdi32
        shell32
        shlwapi
        ole32
        oleaut32
        advapi32
        ws2_32
        crypt32
        secur32
        psapi
        iphlpapi
        wbemuuid
        version
        d3d11
        dxgi
)

set_target_properties(
        ${APP_NAME}
        PROPERTIES
        OUTPUT_NAME "${APP_OUTPUT_NAME}"
        WIN32_EXECUTABLE TRUE
)

if(EXISTS "${PROJECT_ROOT}/assets")
    add_custom_command(
            TARGET ${APP_NAME}
            POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                    "${PROJECT_ROOT}/assets"
                    "$<TARGET_FILE_DIR:${APP_NAME}>/assets"
            COMMENT "[CEH] 正在同步 assets ..."
    )
endif()

if(EXISTS "${PROJECT_ROOT}/runtime/Windows")
    add_custom_command(
            TARGET ${APP_NAME}
            POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                    "${PROJECT_ROOT}/runtime/Windows"
                    "$<TARGET_FILE_DIR:${APP_NAME}>/runtime/Windows"
            COMMENT "[CEH] 正在同步 llama.cpp Windows Runtime ..."
    )
endif()

if(WIN32)
    add_custom_command(
            TARGET ${APP_NAME}
            POST_BUILD
            COMMAND cmd.exe /c
                    "if exist \"${OPENCV_DLL_BASE_DIR}\\opencv_world4120$<$<CONFIG:Debug>:d>.dll\" copy /Y \"${OPENCV_DLL_BASE_DIR}\\opencv_world4120$<$<CONFIG:Debug>:d>.dll\" \"$<TARGET_FILE_DIR:${APP_NAME}>\\\" >nul 2>nul"
            COMMENT "[CEH] 正在部署 OpenCV DLL ..."
    )

    add_custom_command(
            TARGET ${APP_NAME}
            POST_BUILD
            COMMAND "${QT_DIR_PATH}/bin/windeployqt.exe"
                    --no-translations
                    --no-compiler-runtime
                    "$<IF:$<CONFIG:Debug>,--debug,--release>"
                    "$<TARGET_FILE:${APP_NAME}>"
            COMMENT "[CEH] 正在执行 windeployqt ..."
    )
endif()

message(STATUS "")
message(STATUS "================================================")
message(STATUS " 木头自动系统控制 / CEH Windows UIA")
message(STATUS "================================================")
message(STATUS "Source       : ${PROJECT_ROOT}")
message(STATUS "EXE          : ${APP_OUTPUT_NAME}.exe")
message(STATUS "Version      : ${APP_VERSION}")
message(STATUS "Qt           : ${QT_DIR_PATH}")
message(STATUS "OpenCV       : ${OpenCV_VERSION}")
message(STATUS "================================================")
message(STATUS "")
