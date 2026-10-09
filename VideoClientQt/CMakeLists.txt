cmake_minimum_required(VERSION 3.16)

project(VideoClientQt VERSION 0.1 LANGUAGES CXX)

# MSVC 默认按 GBK 读源文件,中文注释会报 C4819;强制按 UTF-8 编译。
if(MSVC)
    add_compile_options(/utf-8)
endif()

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets LinguistTools)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets LinguistTools)

set(TS_FILES VideoClientQt_zh_CN.ts)

set(PROJECT_SOURCES
        main.cpp
        mainwindow.cpp
        mainwindow.h
        mainwindow.ui
        ${TS_FILES}
)

if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    qt_add_executable(VideoClientQt
        MANUAL_FINALIZATION
        ${PROJECT_SOURCES}
    )
# Define target properties for Android with Qt 6 as:
#    set_property(TARGET VideoClientQt APPEND PROPERTY QT_ANDROID_PACKAGE_SOURCE_DIR
#                 ${CMAKE_CURRENT_SOURCE_DIR}/android)
# For more information, see https://doc.qt.io/qt-6/qt-add-executable.html#target-creation

    qt_create_translation(QM_FILES ${CMAKE_SOURCE_DIR} ${TS_FILES})
else()
    if(ANDROID)
        add_library(VideoClientQt SHARED
            ${PROJECT_SOURCES}
        )
# Define properties for Android with Qt 5 after find_package() calls as:
#    set(ANDROID_PACKAGE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/android")
    else()
        add_executable(VideoClientQt
            ${PROJECT_SOURCES}
            evla.h evla.cpp
        )
    endif()

    qt5_create_translation(QM_FILES ${CMAKE_SOURCE_DIR} ${TS_FILES})
endif()

target_link_libraries(VideoClientQt PRIVATE Qt${QT_VERSION_MAJOR}::Widgets)

# ============ VLC 库配置(编译+链接+运行时拷贝) ============
# 四件套已复制进本工程:include\vlc\(头文件)、lib\(导入库)、libvlc.dll/libvlccore.dll、plugins\
# 这样工程自包含:整个 VideoClientQt 文件夹拷走就能编译运行,不依赖隔壁 VideoPlay 工程。

# ① 编译时:头文件目录,让 #include <vlc/vlc.h> 找得到
target_include_directories(VideoClientQt PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

# ② 链接时:导入库(.lib)所在目录
target_link_directories(VideoClientQt PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/lib"
)

# ③ 链接时:链接 libvlc.lib / libvlccore.lib(写不带 .lib 后缀,CMake 自动补)
#    注意:这是 32 位库,Kit 必须用 Qt 5.15.2 MSVC2019 32bit;换 64 位 Kit 会 LNK1112
target_link_libraries(VideoClientQt PRIVATE libvlc libvlccore)

# ④ 运行时:每次编译成功,自动把 dll 和 plugins 复制到 exe 旁边
#    替代了 VideoPlay 里"手动拷 dll"的活;没这步运行会报"找不到 libvlc.dll"
add_custom_command(TARGET VideoClientQt POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/libvlc.dll"
        "$<TARGET_FILE_DIR:VideoClientQt>"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/libvlccore.dll"
        "$<TARGET_FILE_DIR:VideoClientQt>"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/plugins"
        "$<TARGET_FILE_DIR:VideoClientQt>/plugins"
)

# Qt for iOS sets MACOSX_BUNDLE_GUI_IDENTIFIER automatically since Qt 6.1.
# If you are developing for iOS or macOS you should consider setting an
# explicit, fixed bundle identifier manually though.
if(${QT_VERSION} VERSION_LESS 6.1.0)
  set(BUNDLE_ID_OPTION MACOSX_BUNDLE_GUI_IDENTIFIER com.example.VideoClientQt)
endif()
set_target_properties(VideoClientQt PROPERTIES
    ${BUNDLE_ID_OPTION}
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

include(GNUInstallDirs)
install(TARGETS VideoClientQt
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(VideoClientQt)
endif()
