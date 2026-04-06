include(FetchContent)

# 自動下載 GoogleTest（不需要手動安裝）
FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
)
# 防止 GTest 覆蓋 Windows CRT 設定
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

enable_testing()

add_executable(DiskVizTests
    main.cpp
    test_FileClassifier.cpp
    test_FormatUtils.cpp
    test_CacheManager.cpp
)

target_link_libraries(DiskVizTests PRIVATE
    GTest::gtest
    Qt6::Core
    Qt6::Sql
    # 直接連結 DiskViz 的核心原始碼（不是整個 executable）
    DiskVizCore
)

target_include_directories(DiskVizTests PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include/core
    ${CMAKE_SOURCE_DIR}/include/utils
)

include(GoogleTest)
gtest_discover_tests(DiskVizTests)
