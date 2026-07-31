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
    app.rc
)

add_executable(MusicPlayer WIN32 ${SOURCES} icon.ico)

# Ensure resource recompilation when icon changes
set_source_files_properties(app.rc PROPERTIES OBJECT_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/icon.ico")

# MSVC: UTF-8 source encoding (supports Chinese chars in string literals)
if (MSVC)
    target_compile_options(MusicPlayer PRIVATE /utf-8)
endif()

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
