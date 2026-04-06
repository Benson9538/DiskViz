cmake_minimum_required(VERSION 3.20)

project(DiskViz VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Widgets Charts Sql Network Concurrent)

# ── 核心模組（library）────────────────────────────────────
# 抽出來讓測試可以連結，不用把整個 executable 拉進來
add_library(DiskVizCore STATIC
    src/core/FileScanner.cpp
    src/core/ProgramFilesProvider.cpp
    src/core/FileClassifier.cpp
    src/core/SizeCalculator.cpp
    src/core/CacheManager.cpp
    src/core/ScanWorker.cpp
    src/core/OllamaClassifier.cpp
    src/utils/FormatUtils.cpp
    src/utils/DriveUtils.cpp
)

target_link_libraries(DiskVizCore PUBLIC
    Qt6::Core
    Qt6::Sql
    Qt6::Network
    Qt6::Concurrent
)

target_include_directories(DiskVizCore PUBLIC
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include/core
    ${CMAKE_SOURCE_DIR}/include/utils
)

# ── 主程式 ────────────────────────────────────────────────
add_executable(DiskViz
    src/main.cpp
    src/ui/MainWindow.cpp
)

target_link_libraries(DiskViz PRIVATE
    DiskVizCore
    Qt6::Widgets
    Qt6::Charts
)

target_include_directories(DiskViz PRIVATE
    ${CMAKE_SOURCE_DIR}/include/ui
)

# ── 測試 ──────────────────────────────────────────────────
add_subdirectory(tests)
