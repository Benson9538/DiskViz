cmake_minimum_required(VERSION 3.20)

project(DiskViz VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Widgets Charts Sql Network)

add_executable(DiskViz
    src/main.cpp
    src/core/FileScanner.cpp
    src/core/ProgramFilesProvider.cpp
    src/core/FileClassifier.cpp
    src/core/SizeCalculator.cpp
    src/core/CacheManager.cpp
    src/core/ScanWorker.cpp
    src/core/OllamaClassifier.cpp
    src/utils/FormatUtils.cpp
    src/utils/DriveUtils.cpp
    src/ui/MainWindow.cpp
)

target_link_libraries(DiskViz PRIVATE
    Qt6::Core
    Qt6::Widgets
    Qt6::Charts
    Qt6::Sql
    Qt6::Network
)

target_include_directories(DiskViz PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include/core
    ${CMAKE_SOURCE_DIR}/include/utils
    ${CMAKE_SOURCE_DIR}/include/ui
)