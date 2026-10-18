cmake_minimum_required(VERSION 3.26)

# ==============================================================================
# 1. Project
# ==============================================================================
project(
    CEH_SocialRelations
    VERSION 1.0.0
    LANGUAGES CXX
)

if(WIN32)
    enable_language(RC)
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 彻底根除 MSVC 中文字符串乱码与保存失败 Bug
if(MSVC)
    add_compile_options(/utf-8)
endif()

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ==============================================================================
# 2. 软件信息
# ==============================================================================
set(APP_TARGET_NAME SocialRelations)
set(APP_OUTPUT_NAME "云萍录")
set(APP_DISPLAY_NAME "云萍录")
set(APP_VERSION "${PROJECT_VERSION}")

# ==============================================================================
# 3. 依赖库显式绝对路径加载 (严禁修改全局环境变量)
# ==============================================================================
if(WIN32)
    # Qt 6.11.1
    set(CEH_QT_ROOT "C:/Qt/6.11.1/msvc2022_64" CACHE PATH "Qt 6 MSVC x64 root")
    if(NOT EXISTS "${CEH_QT_ROOT}")
        message(FATAL_ERROR "[Qt] Qt 根目录不存在：${CEH_QT_ROOT}")
    endif()
    list(PREPEND CMAKE_PREFIX_PATH "${CEH_QT_ROOT}")
endif()

find_package(
    Qt6
    REQUIRED
    COMPONENTS
    Core
    Gui
    Widgets
    Sql
    Concurrent
    Network
)

message(STATUS "[Qt] Found Qt Version = ${Qt6_VERSION} at ${CEH_QT_ROOT}")

# ==============================================================================
# 4. 源码组织
# ==============================================================================
set(PROJECT_ROOT "${CMAKE_CURRENT_SOURCE_DIR}")
set(ASSETS_SOURCE_DIR "${PROJECT_ROOT}/assets")
set(PLATFORM_WINDOWS_DIR "${PROJECT_ROOT}/platform/Windows")

include_directories(
    "${PROJECT_ROOT}"
    "${PROJECT_ROOT}/src"
)

set(APP_SOURCES
    "${PROJECT_ROOT}/main.cpp"
    "${PROJECT_ROOT}/resources.qrc"

    # Core
    "${PROJECT_ROOT}/src/core/DatabaseManager.h"
    "${PROJECT_ROOT}/src/core/DatabaseManager.cpp"
    "${PROJECT_ROOT}/src/core/VcfParser.h"
    "${PROJECT_ROOT}/src/core/VcfParser.cpp"
    "${PROJECT_ROOT}/src/core/SecurityManager.h"
    "${PROJECT_ROOT}/src/core/SecurityManager.cpp"

    # AI
    "${PROJECT_ROOT}/src/ai/ILlmEngine.h"
    "${PROJECT_ROOT}/src/ai/OllamaHttpEngine.h"
    "${PROJECT_ROOT}/src/ai/OllamaHttpEngine.cpp"
    "${PROJECT_ROOT}/src/ai/AiAdvisorService.h"
    "${PROJECT_ROOT}/src/ai/AiAdvisorService.cpp"

    # UI
    "${PROJECT_ROOT}/src/ui/TopBar.h"
    "${PROJECT_ROOT}/src/ui/TopBar.cpp"
    "${PROJECT_ROOT}/src/ui/FramelessWindow.h"
    "${PROJECT_ROOT}/src/ui/FramelessWindow.cpp"
    "${PROJECT_ROOT}/src/ui/GeneralDashboardWidget.h"
    "${PROJECT_ROOT}/src/ui/GeneralDashboardWidget.cpp"
    "${PROJECT_ROOT}/src/ui/FinancialLedgerWidget.h"
    "${PROJECT_ROOT}/src/ui/FinancialLedgerWidget.cpp"
    "${PROJECT_ROOT}/src/ui/LedgerWidget.h"
    "${PROJECT_ROOT}/src/ui/LedgerWidget.cpp"
    "${PROJECT_ROOT}/src/ui/DebtWidget.h"
    "${PROJECT_ROOT}/src/ui/DebtWidget.cpp"
    "${PROJECT_ROOT}/src/ui/ContactWidget.h"
    "${PROJECT_ROOT}/src/ui/ContactWidget.cpp"
    "${PROJECT_ROOT}/src/ui/ComparisonChartWidget.h"
    "${PROJECT_ROOT}/src/ui/ComparisonChartWidget.cpp"
    "${PROJECT_ROOT}/src/ui/RelationshipDiaryDialog.h"
    "${PROJECT_ROOT}/src/ui/RelationshipDiaryDialog.cpp"
    "${PROJECT_ROOT}/src/ui/CorporateFinanceWidget.h"
    "${PROJECT_ROOT}/src/ui/CorporateFinanceWidget.cpp"
    "${PROJECT_ROOT}/src/ui/PersonalFinanceWidget.h"
    "${PROJECT_ROOT}/src/ui/PersonalFinanceWidget.cpp"
    "${PROJECT_ROOT}/src/ui/AssetWidget.h"
    "${PROJECT_ROOT}/src/ui/AssetWidget.cpp"
    "${PROJECT_ROOT}/src/ui/RelationGraphWidget.h"
    "${PROJECT_ROOT}/src/ui/RelationGraphWidget.cpp"
    "${PROJECT_ROOT}/src/ui/BusinessNetWidget.h"
    "${PROJECT_ROOT}/src/ui/BusinessNetWidget.cpp"
    "${PROJECT_ROOT}/src/ui/AiAdvisorPanel.h"
    "${PROJECT_ROOT}/src/ui/AiAdvisorPanel.cpp"
    "${PROJECT_ROOT}/src/ui/ReminderDialog.h"
    "${PROJECT_ROOT}/src/ui/ReminderDialog.cpp"
    "${PROJECT_ROOT}/src/ui/LoginWindow.h"
    "${PROJECT_ROOT}/src/ui/LoginWindow.cpp"
    "${PROJECT_ROOT}/src/ui/CustomDialog.h"
    "${PROJECT_ROOT}/src/ui/CustomDialog.cpp"
    "${PROJECT_ROOT}/src/ui/UnifiedTransactionDialog.h"
    "${PROJECT_ROOT}/src/ui/UnifiedTransactionDialog.cpp"
    "${PROJECT_ROOT}/src/ui/SocialPlanDialog.h"
    "${PROJECT_ROOT}/src/ui/SocialPlanDialog.cpp"
    "${PROJECT_ROOT}/src/ui/ModelSettingsDialog.h"
    "${PROJECT_ROOT}/src/ui/ModelSettingsDialog.cpp"
    "${PROJECT_ROOT}/src/ui/SecuritySettingsDialog.h"
    "${PROJECT_ROOT}/src/ui/SecuritySettingsDialog.cpp"
    "${PROJECT_ROOT}/src/ui/UserProfileDialog.h"
    "${PROJECT_ROOT}/src/ui/UserProfileDialog.cpp"
    "${PROJECT_ROOT}/src/ui/CompanyDialog.h"
    "${PROJECT_ROOT}/src/ui/CompanyDialog.cpp"
    "${PROJECT_ROOT}/src/ui/ContactDialog.h"
    "${PROJECT_ROOT}/src/ui/ContactDialog.cpp"
    "${PROJECT_ROOT}/src/ui/MainWindow.h"
    "${PROJECT_ROOT}/src/ui/MainWindow.cpp"
)

if(WIN32)
    set(WINDOWS_MANIFEST_SOURCE "${PLATFORM_WINDOWS_DIR}/app.manifest")
    set(WINDOWS_RC_SOURCE "${PLATFORM_WINDOWS_DIR}/app.rc")
    set(WINDOWS_PERMISSION_CPP "${PLATFORM_WINDOWS_DIR}/Permission.cpp")
    set(WINDOWS_ICON_SOURCE "${ASSETS_SOURCE_DIR}/img/icon/favicon_128x128.ico")

    if(NOT EXISTS "${WINDOWS_ICON_SOURCE}")
        set(WINDOWS_ICON_SOURCE "${PLATFORM_WINDOWS_DIR}/wood_app_icon.ico")
    endif()

    set(WINDOWS_BUILD_ICON "${CMAKE_BINARY_DIR}/wood_app_icon.ico")
    set(WINDOWS_BUILD_MANIFEST "${CMAKE_BINARY_DIR}/wood_app.manifest")
    set(WINDOWS_BUILD_RC "${CMAKE_BINARY_DIR}/wood_app.rc")

    configure_file("${WINDOWS_ICON_SOURCE}" "${WINDOWS_BUILD_ICON}" COPYONLY)
    configure_file("${WINDOWS_MANIFEST_SOURCE}" "${WINDOWS_BUILD_MANIFEST}" COPYONLY)
    file(TO_CMAKE_PATH "${WINDOWS_BUILD_ICON}" WINDOWS_BUILD_ICON_CMAKE)

    file(WRITE "${WINDOWS_BUILD_RC}"
"#pragma code_page(65001)
#include <windows.h>
#define IDI_APP_ICON 101
IDI_APP_ICON ICON \"${WINDOWS_BUILD_ICON_CMAKE}\"
VS_VERSION_INFO VERSIONINFO
 FILEVERSION 1,0,0,0
 PRODUCTVERSION 1,0,0,0
 FILEFLAGSMASK 0x3fL
#ifdef _DEBUG
 FILEFLAGS VS_FF_DEBUG
#else
 FILEFLAGS 0x0L
#endif
 FILEOS VOS_NT_WINDOWS32
 FILETYPE VFT_APP
 FILESUBTYPE VFT2_UNKNOWN
BEGIN
    BLOCK \"StringFileInfo\"
    BEGIN
        BLOCK \"080404B0\"
        BEGIN
            VALUE \"CompanyName\", \"台州木头智能科技有限公司\\0\"
            VALUE \"FileDescription\", \"云萍录 - 社会关系与财务参谋\\0\"
            VALUE \"FileVersion\", \"${APP_VERSION}\\0\"
            VALUE \"InternalName\", \"${APP_TARGET_NAME}\\0\"
            VALUE \"LegalCopyright\", \"Copyright (C) 2026 台州木头智能科技有限公司\\0\"
            VALUE \"OriginalFilename\", \"${APP_OUTPUT_NAME}.exe\\0\"
            VALUE \"ProductName\", \"云萍录（社会关系与财务参谋）\\0\"
            VALUE \"ProductVersion\", \"${APP_VERSION}\\0\"
        END
    END
    BLOCK \"VarFileInfo\"
    BEGIN
        VALUE \"Translation\", 0x0804, 1200
    END
END
")

    list(APPEND APP_SOURCES
        "${WINDOWS_PERMISSION_CPP}"
        "${WINDOWS_BUILD_RC}"
        "${WINDOWS_BUILD_MANIFEST}"
    )
endif()

# ==============================================================================
# 5. 可执行文件目标
# ==============================================================================
qt_add_executable(
    ${APP_TARGET_NAME}
    WIN32
    ${APP_SOURCES}
)

set_target_properties(
    ${APP_TARGET_NAME}
    PROPERTIES
    OUTPUT_NAME "${APP_OUTPUT_NAME}"
)

target_link_libraries(
    ${APP_TARGET_NAME}
    PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Sql
    Qt6::Concurrent
    Qt6::Network
)

if(WIN32)
    target_link_libraries(
        ${APP_TARGET_NAME}
        PRIVATE
        user32
        shell32
        ole32
        advapi32
        dwmapi
        version
    )
endif()

# ==============================================================================
# 6. Windows 独立部署与完整性校验 (Qt6 + SQLite + llama.cpp Runtime + VC_redist + Docs)
# ==============================================================================
if(WIN32)
    find_program(
        QT_WINDEPLOYQT_EXECUTABLE
        NAMES windeployqt windeployqt.exe
        PATHS "${CEH_QT_ROOT}/bin"
        NO_DEFAULT_PATH
    )
    if(NOT QT_WINDEPLOYQT_EXECUTABLE)
        message(FATAL_ERROR "[Qt] 未找到 windeployqt.exe：${CEH_QT_ROOT}/bin")
    endif()

    set(RUNTIME_WINDOWS_SOURCE_DIR "${PROJECT_ROOT}/runtime/Windows")
    set(COMPILATION_SOURCE_DIR "${PROJECT_ROOT}/Compilation")
    set(DEPLOY_SCRIPT "${CMAKE_BINARY_DIR}/deploy_standalone.cmake")
    set(VERIFY_SCRIPT "${CMAKE_BINARY_DIR}/verify_standalone_deploy.cmake")

    file(WRITE "${DEPLOY_SCRIPT}" [=[
if(NOT DEFINED TARGET_EXE OR NOT EXISTS "${TARGET_EXE}")
    message(FATAL_ERROR "[DEPLOY] 目标可执行文件不存在: ${TARGET_EXE}")
endif()

get_filename_component(TARGET_DIR "${TARGET_EXE}" DIRECTORY)

string(TOLOWER "${BUILD_CONFIG}" _cfg_lower)
set(_is_debug FALSE)
if(_cfg_lower STREQUAL "debug")
    set(_is_debug TRUE)
endif()

if(_is_debug)
    set(_windeploy_mode "--debug")
    set(_qt_suffix "d")
else()
    set(_windeploy_mode "--release")
    set(_qt_suffix "")
endif()

message(STATUS "[DEPLOY] 1/5 运行 windeployqt 部署 Qt 运行时与插件 (${_windeploy_mode})...")
execute_process(
    COMMAND "${QT_WINDEPLOYQT}"
        ${_windeploy_mode}
        --no-translations
        --no-system-d3d-compiler
        --no-opengl-sw
        --dir "${TARGET_DIR}"
        "${TARGET_EXE}"
    RESULT_VARIABLE _windeploy_res
)
if(NOT _windeploy_res EQUAL 0)
    message(WARNING "[DEPLOY] windeployqt 返回非零状态 (${_windeploy_res})，继续执行显式补齐...")
endif()

set(_qt_core_modules Core Gui Widgets Sql Concurrent Network Svg Pdf)
foreach(_mod IN LISTS _qt_core_modules)
    set(_dll_name "Qt6${_mod}${_qt_suffix}.dll")
    set(_src_dll "${QT_ROOT}/bin/${_dll_name}")
    set(_dst_dll "${TARGET_DIR}/${_dll_name}")
    if(EXISTS "${_src_dll}")
        file(COPY_FILE "${_src_dll}" "${_dst_dll}" ONLY_IF_DIFFERENT)
    endif()
endforeach()

file(MAKE_DIRECTORY "${TARGET_DIR}/platforms")
if(EXISTS "${QT_ROOT}/plugins/platforms/qwindows${_qt_suffix}.dll")
    file(COPY_FILE
        "${QT_ROOT}/plugins/platforms/qwindows${_qt_suffix}.dll"
        "${TARGET_DIR}/platforms/qwindows${_qt_suffix}.dll"
        ONLY_IF_DIFFERENT
    )
endif()

file(MAKE_DIRECTORY "${TARGET_DIR}/sqldrivers")
if(EXISTS "${QT_ROOT}/plugins/sqldrivers/qsqlite${_qt_suffix}.dll")
    file(COPY_FILE
        "${QT_ROOT}/plugins/sqldrivers/qsqlite${_qt_suffix}.dll"
        "${TARGET_DIR}/sqldrivers/qsqlite${_qt_suffix}.dll"
        ONLY_IF_DIFFERENT
    )
endif()

file(MAKE_DIRECTORY "${TARGET_DIR}/styles")
if(EXISTS "${QT_ROOT}/plugins/styles/qmodernwindowsstyle${_qt_suffix}.dll")
    file(COPY_FILE
        "${QT_ROOT}/plugins/styles/qmodernwindowsstyle${_qt_suffix}.dll"
        "${TARGET_DIR}/styles/qmodernwindowsstyle${_qt_suffix}.dll"
        ONLY_IF_DIFFERENT
    )
endif()

file(MAKE_DIRECTORY "${TARGET_DIR}/imageformats")
set(_img_plugins qico qjpeg qsvg qgif)
foreach(_p IN LISTS _img_plugins)
    if(EXISTS "${QT_ROOT}/plugins/imageformats/${_p}${_qt_suffix}.dll")
        file(COPY_FILE
            "${QT_ROOT}/plugins/imageformats/${_p}${_qt_suffix}.dll"
            "${TARGET_DIR}/imageformats/${_p}${_qt_suffix}.dll"
            ONLY_IF_DIFFERENT
        )
    endif()
endforeach()

file(MAKE_DIRECTORY "${TARGET_DIR}/tls")
set(_tls_plugins qschannelbackend qcertonlybackend qopensslbackend)
foreach(_tp IN LISTS _tls_plugins)
    if(EXISTS "${QT_ROOT}/plugins/tls/${_tp}${_qt_suffix}.dll")
        file(COPY_FILE
            "${QT_ROOT}/plugins/tls/${_tp}${_qt_suffix}.dll"
            "${TARGET_DIR}/tls/${_tp}${_qt_suffix}.dll"
            ONLY_IF_DIFFERENT
        )
    endif()
endforeach()

file(MAKE_DIRECTORY "${TARGET_DIR}/networkinformation")
if(EXISTS "${QT_ROOT}/plugins/networkinformation/qnetworklistmanager${_qt_suffix}.dll")
    file(COPY_FILE
        "${QT_ROOT}/plugins/networkinformation/qnetworklistmanager${_qt_suffix}.dll"
        "${TARGET_DIR}/networkinformation/qnetworklistmanager${_qt_suffix}.dll"
        ONLY_IF_DIFFERENT
    )
endif()

if(_is_debug)
    set(_stale_release_files
        "${TARGET_DIR}/platforms/qwindows.dll"
        "${TARGET_DIR}/sqldrivers/qsqlite.dll"
        "${TARGET_DIR}/styles/qmodernwindowsstyle.dll"
        "${TARGET_DIR}/imageformats/qgif.dll"
        "${TARGET_DIR}/imageformats/qicns.dll"
        "${TARGET_DIR}/imageformats/qico.dll"
        "${TARGET_DIR}/imageformats/qjpeg.dll"
        "${TARGET_DIR}/imageformats/qpdf.dll"
        "${TARGET_DIR}/imageformats/qsvg.dll"
        "${TARGET_DIR}/imageformats/qtga.dll"
        "${TARGET_DIR}/imageformats/qtiff.dll"
        "${TARGET_DIR}/imageformats/qwbmp.dll"
        "${TARGET_DIR}/imageformats/qwebp.dll"
    )
    foreach(_stale IN LISTS _stale_release_files)
        if(EXISTS "${_stale}")
            file(REMOVE "${_stale}")
        endif()
    endforeach()
else()
    set(_stale_debug_files
        "${TARGET_DIR}/Qt6Cored.dll"
        "${TARGET_DIR}/Qt6Guid.dll"
        "${TARGET_DIR}/Qt6Widgetsd.dll"
        "${TARGET_DIR}/Qt6Sqld.dll"
        "${TARGET_DIR}/Qt6Concurrentd.dll"
        "${TARGET_DIR}/Qt6Networkd.dll"
        "${TARGET_DIR}/platforms/qwindowsd.dll"
        "${TARGET_DIR}/sqldrivers/qsqlited.dll"
        "${TARGET_DIR}/styles/qmodernwindowsstyled.dll"
    )
    foreach(_stale IN LISTS _stale_debug_files)
        if(EXISTS "${_stale}")
            file(REMOVE "${_stale}")
        endif()
    endforeach()
endif()

message(STATUS "[DEPLOY] 2/5 同步 assets 资源目录...")
if(EXISTS "${ASSETS_SRC}")
    if(EXISTS "${TARGET_DIR}/assets/db/wood.db")
        file(COPY "${ASSETS_SRC}/" DESTINATION "${TARGET_DIR}/assets" PATTERN "wood.db*" EXCLUDE)
    else()
        file(COPY "${ASSETS_SRC}/" DESTINATION "${TARGET_DIR}/assets")
    endif()
endif()

message(STATUS "[DEPLOY] 3/5 同步 llama.cpp Windows Runtime (cpu + cuda12)...")
if(EXISTS "${RUNTIME_WIN_SRC}")
    file(MAKE_DIRECTORY "${TARGET_DIR}/runtime/Windows")
    file(COPY "${RUNTIME_WIN_SRC}/" DESTINATION "${TARGET_DIR}/runtime/Windows")
endif()

set(_forbidden_nvcuda
    "${TARGET_DIR}/nvcuda.dll"
    "${TARGET_DIR}/runtime/Windows/cpu/nvcuda.dll"
    "${TARGET_DIR}/runtime/Windows/cuda12/nvcuda.dll"
)
foreach(_nv IN LISTS _forbidden_nvcuda)
    if(EXISTS "${_nv}")
        file(REMOVE "${_nv}")
    endif()
endforeach()

message(STATUS "[DEPLOY] 4/5 同步 VC_redist.x64.exe 与发行说明文档...")
if(EXISTS "${COMPILATION_SRC}/VC_redist.x64.exe")
    file(COPY_FILE
        "${COMPILATION_SRC}/VC_redist.x64.exe"
        "${TARGET_DIR}/VC_redist.x64.exe"
        ONLY_IF_DIFFERENT
    )
endif()

set(_doc_files
    "license.txt"
    "README.txt"
    "安装与硬件要求.txt"
    "THIRD_PARTY_NOTICES.txt"
    "MODEL_NOTICE.txt"
    "CHANGELOG.txt"
)
foreach(_doc IN LISTS _doc_files)
    if(EXISTS "${COMPILATION_SRC}/${_doc}")
        file(COPY_FILE
            "${COMPILATION_SRC}/${_doc}"
            "${TARGET_DIR}/${_doc}"
            ONLY_IF_DIFFERENT
        )
    endif()
endforeach()
]=])

    file(WRITE "${VERIFY_SCRIPT}" [=[
get_filename_component(TARGET_DIR "${TARGET_EXE}" DIRECTORY)

string(TOLOWER "${BUILD_CONFIG}" _cfg_lower)
set(_qt_suffix "")
if(_cfg_lower STREQUAL "debug")
    set(_qt_suffix "d")
endif()

set(_required_files
    "${TARGET_EXE}"
    "${TARGET_DIR}/Qt6Core${_qt_suffix}.dll"
    "${TARGET_DIR}/Qt6Gui${_qt_suffix}.dll"
    "${TARGET_DIR}/Qt6Widgets${_qt_suffix}.dll"
    "${TARGET_DIR}/Qt6Sql${_qt_suffix}.dll"
    "${TARGET_DIR}/Qt6Concurrent${_qt_suffix}.dll"
    "${TARGET_DIR}/Qt6Network${_qt_suffix}.dll"
    "${TARGET_DIR}/platforms/qwindows${_qt_suffix}.dll"
    "${TARGET_DIR}/sqldrivers/qsqlite${_qt_suffix}.dll"
    "${TARGET_DIR}/assets/style/main.qss"
    "${TARGET_DIR}/assets/img/icon/favicon_128x128.ico"
    "${TARGET_DIR}/assets/db/wood.db"
    "${TARGET_DIR}/VC_redist.x64.exe"
    "${TARGET_DIR}/runtime/Windows/cpu/llama-server.exe"
    "${TARGET_DIR}/runtime/Windows/cpu/llama.dll"
    "${TARGET_DIR}/runtime/Windows/cpu/ggml.dll"
    "${TARGET_DIR}/runtime/Windows/cpu/ggml-base.dll"
    "${TARGET_DIR}/runtime/Windows/cuda12/llama-server.exe"
    "${TARGET_DIR}/runtime/Windows/cuda12/llama.dll"
    "${TARGET_DIR}/runtime/Windows/cuda12/ggml-cuda.dll"
)

set(_missing "")
foreach(_f IN LISTS _required_files)
    if(NOT EXISTS "${_f}")
        list(APPEND _missing "${_f}")
    endif()
endforeach()

if(_missing)
    string(REPLACE ";" "\n  - " _missing_str "${_missing}")
    message(FATAL_ERROR "[VERIFY] 独立运行目录缺少关键组件:\n  - ${_missing_str}")
endif()

message(STATUS "[VERIFY] 独立运行目录完整性验证通过: ${TARGET_DIR}")
]=])

    add_custom_command(
        TARGET ${APP_TARGET_NAME} PRE_LINK
        COMMAND cmd /c "taskkill.exe /F /IM \"${APP_OUTPUT_NAME}.exe\" 2>nul & taskkill.exe /F /IM \"${APP_TARGET_NAME}.exe\" 2>nul & exit 0"
        COMMENT "[PRE_LINK] 自动清理可能占用 exe 的运行中进程以杜绝 LNK1104 / LNK1168 链接锁定..."
    )

    add_custom_command(
        TARGET ${APP_TARGET_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND}
            -DTARGET_EXE="$<TARGET_FILE:${APP_TARGET_NAME}>"
            -DBUILD_CONFIG="$<CONFIG>"
            -DQT_ROOT="${CEH_QT_ROOT}"
            -DQT_WINDEPLOYQT="${QT_WINDEPLOYQT_EXECUTABLE}"
            -DASSETS_SRC="${ASSETS_SOURCE_DIR}"
            -DRUNTIME_WIN_SRC="${RUNTIME_WINDOWS_SOURCE_DIR}"
            -DCOMPILATION_SRC="${COMPILATION_SOURCE_DIR}"
            -P "${DEPLOY_SCRIPT}"
        COMMAND ${CMAKE_COMMAND}
            -DTARGET_EXE="$<TARGET_FILE:${APP_TARGET_NAME}>"
            -DBUILD_CONFIG="$<CONFIG>"
            -P "${VERIFY_SCRIPT}"
        COMMENT "[DEPLOY & VERIFY] 自动部署 Qt6 / SQLite / assets / llama.cpp Runtime 并验证独立运行目录..."
    )
endif()
