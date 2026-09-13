cmake_minimum_required(VERSION 3.16)
project(MusicPlayer)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ============================================
# BASS Audio Library Configuration
# ============================================
# Download BASS v2.4 from: https://www.un4seen.com/bass.html
# Extract the ZIP and place contents in project's "bass" directory:
#
#   ${PROJECT_SOURCE_DIR}/bass/include/bass.h
#   ${PROJECT_SOURCE_DIR}/bass/lib/x64/bass.lib
#   ${PROJECT_SOURCE_DIR}/bass/lib/x64/bass.dll
#
# Alternatively, set BASS_DIR to point to your BASS installation:
#   cmake -DBASS_DIR="C:/path/to/bass" ..
# ============================================

if (NOT DEFINED BASS_DIR)
    set(BASS_DIR "${PROJECT_SOURCE_DIR}/bass")
endif()

if (CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(BASS_LIB_DIR "${BASS_DIR}/lib/x64")
else()
    set(BASS_LIB_DIR "${BASS_DIR}/lib/x86")
endif()

include_directories("${BASS_DIR}/include")
link_directories("${BASS_LIB_DIR}")

# Sources
set(SOURCES
    main.cpp
    AudioEngine.h
    AudioEngine.cpp
    PlaylistManager.h
    PlaylistManager.cpp
    ListeningHistory.h
    ListeningHistory.cpp
    LyricParser.h
    LyricParser.cpp
    LyricWindow.h
    LyricWindow.cpp
    LyricsController.h
    LyricsController.cpp
    DurationCache.h
    DurationCache.cpp
    Settings.h
    Settings.cpp
    TrayIcon.h
    TrayIcon.cpp
    Dialogs.h
    Dialogs.cpp
    Hotkey.h
    Hotkey.cpp
    TextFile.h
    TextFile.cpp
    app.rc
)

add_executable(MusicPlayer WIN32 ${SOURCES} icon.ico)

# Ensure resource recompilation when icon changes
set_source_files_properties(app.rc PROPERTIES OBJECT_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/icon.ico")

# MSVC: UTF-8 source encoding (supports Chinese chars in string literals)
if (MSVC)
    target_compile_options(MusicPlayer PRIVATE /utf-8)
endif()

# MinGW 发布相关
if (MINGW)
    # app.rc 里的版本信息含中文, 指定输入代码页为 UTF-8 以免乱码
    set(CMAKE_RC_FLAGS "${CMAKE_RC_FLAGS} --codepage=65001")
    # 静态链接 gcc 运行库, 把运行时依赖从 3 个 DLL 减到 1 个。
    # 不用整体 -static: ld 会拒绝 bass/bass_fx 的导入库(.lib 只有 DLL 导入库版本),
    # 且会引入 __intrinsic_setjmpex / __ms_vsnprintf 等静态运行库符号冲突。
    # 静态 libstdc++ 仍会引用 winpthread, 故 libwinpthread-1.dll 需随包分发。
    target_link_options(MusicPlayer PRIVATE -static-libgcc -static-libstdc++)

    get_filename_component(MINGW_BIN_DIR "${CMAKE_CXX_COMPILER}" DIRECTORY)
    set(MINGW_RUNTIME_DLL "${MINGW_BIN_DIR}/libwinpthread-1.dll")

    add_custom_command(TARGET MusicPlayer POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${MINGW_RUNTIME_DLL}"
        "$<TARGET_FILE_DIR:MusicPlayer>"
        COMMENT "Copying libwinpthread-1.dll to output directory..."
    )
endif()

# Release: 去掉符号表以精简体积
target_link_options(MusicPlayer PRIVATE $<$<CONFIG:Release>:-s>)

# Link Libraries
target_link_libraries(MusicPlayer
    bass.lib
    bass_fx.lib
    winmm.lib
    comctl32.lib
    ole32.lib
    shell32.lib
    user32.lib
    gdi32.lib
    gdiplus.lib
    oleaut32.lib
)

# Copy BASS DLLs to output directory
add_custom_command(TARGET MusicPlayer POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${BASS_LIB_DIR}/bass.dll"
    "$<TARGET_FILE_DIR:MusicPlayer>"
    COMMENT "Copying bass.dll to output directory..."
)
add_custom_command(TARGET MusicPlayer POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${BASS_LIB_DIR}/bass_fx.dll"
    "$<TARGET_FILE_DIR:MusicPlayer>"
    COMMENT "Copying bass_fx.dll to output directory..."
)
add_custom_command(TARGET MusicPlayer POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${BASS_LIB_DIR}/bassflac.dll"
    "$<TARGET_FILE_DIR:MusicPlayer>"
    COMMENT "Copying bassflac.dll to output directory..."
)

# ============================================
# 发布暂存 (供 Inno Setup 打包)
# ============================================
# 用法: cmake --install <build-dir> --prefix dist
# 只装 exe 与 BASS 运行时 DLL; Data\ 状态文件与 .error.log 是运行时产物, 绝不打包
install(TARGETS MusicPlayer RUNTIME DESTINATION .)
install(FILES
    "${BASS_LIB_DIR}/bass.dll"
    "${BASS_LIB_DIR}/bass_fx.dll"
    "${BASS_LIB_DIR}/bassflac.dll"
    DESTINATION .)
if (MINGW)
    install(FILES "${MINGW_RUNTIME_DLL}" DESTINATION .)
endif()
