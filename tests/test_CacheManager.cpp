#include <gtest/gtest.h>
#include <QCoreApplication>
#include "core/CacheManager.h"

class CacheManagerTest : public ::testing::Test {
protected:
    CacheManager* cache;

    void SetUp() override {
        cache = new CacheManager();
        // 使用記憶體資料庫，測試結束後自動消失，不影響真實快取
        cache->init(":memory:");
    }

    void TearDown() override {
        delete cache;
    }
};

TEST_F(CacheManagerTest, HasCacheReturnsFalseWhenEmpty) {
    EXPECT_FALSE(cache->hasCache("/test/path"));
}

TEST_F(CacheManagerTest, SaveAndLoadEntries) {
    std::vector<CachedEntry> entries;
    CachedEntry e;
    e.rootPath = "/test";
    e.path     = "/test/file.txt";
    e.size     = 1024;
    e.category = "文件";
    e.isDir    = false;
    entries.push_back(e);

    cache->saveEntries("/test", entries);

    EXPECT_TRUE(cache->hasCache("/test"));

    auto loaded = cache->loadEntries("/test");
    ASSERT_EQ(loaded.size(), 1u);
    EXPECT_EQ(loaded[0].path,     "/test/file.txt");
    EXPECT_EQ(loaded[0].size,     1024);
    EXPECT_EQ(loaded[0].category, "文件");
    EXPECT_FALSE(loaded[0].isDir);
}

TEST_F(CacheManagerTest, UpdateSize) {
    std::vector<CachedEntry> entries;
    CachedEntry e;
    e.rootPath = "/test";
    e.path     = "/test/folder";
    e.size     = 0;
    e.category = "未知";
    e.isDir    = true;
    entries.push_back(e);

    cache->saveEntries("/test", entries);
    cache->updateSize("/test/folder", 9999);

    auto loaded = cache->loadEntries("/test");
    ASSERT_EQ(loaded.size(), 1u);
    EXPECT_EQ(loaded[0].size, 9999);
}

TEST_F(CacheManagerTest, UpdateCategory) {
    std::vector<CachedEntry> entries;
    CachedEntry e;
    e.rootPath = "/test";
    e.path     = "/test/file.bin";
    e.size     = 512;
    e.category = "未知";
    e.isDir    = false;
    entries.push_back(e);

    cache->saveEntries("/test", entries);
    cache->updateCategory("/test/file.bin", "應用程式");

    auto loaded = cache->loadEntries("/test");
    ASSERT_EQ(loaded.size(), 1u);
    EXPECT_EQ(loaded[0].category, "應用程式");
}

TEST_F(CacheManagerTest, SaveOverwritesPreviousCache) {
    std::vector<CachedEntry> first;
    CachedEntry e1;
    e1.rootPath = "/test"; e1.path = "/test/a"; e1.size = 100;
    e1.category = "文件"; e1.isDir = false;
    first.push_back(e1);
    cache->saveEntries("/test", first);

    std::vector<CachedEntry> second;
    CachedEntry e2;
    e2.rootPath = "/test"; e2.path = "/test/b"; e2.size = 200;
    e2.category = "影片"; e2.isDir = false;
    second.push_back(e2);
    cache->saveEntries("/test", second);

    auto loaded = cache->loadEntries("/test");
    // 舊資料應該被清掉，只剩新的一筆
    ASSERT_EQ(loaded.size(), 1u);
    EXPECT_EQ(loaded[0].path, "/test/b");
}
