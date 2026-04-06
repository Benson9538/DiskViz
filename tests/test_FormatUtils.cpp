#include <gtest/gtest.h>
#include "utils/FormatUtils.h"

TEST(FormatUtilsTest, Bytes) {
    EXPECT_EQ(FormatUtils::formatSize(0),   "0 B");
    EXPECT_EQ(FormatUtils::formatSize(1),   "1 B");
    EXPECT_EQ(FormatUtils::formatSize(512), "512 B");
    EXPECT_EQ(FormatUtils::formatSize(1023),"1023 B");
}

TEST(FormatUtilsTest, Kilobytes) {
    EXPECT_EQ(FormatUtils::formatSize(1024),     "1.0 KB");
    EXPECT_EQ(FormatUtils::formatSize(1536),     "1.5 KB");
    EXPECT_EQ(FormatUtils::formatSize(1024 * 10),"10.0 KB");
}

TEST(FormatUtilsTest, Megabytes) {
    EXPECT_EQ(FormatUtils::formatSize(1024LL * 1024),       "1.0 MB");
    EXPECT_EQ(FormatUtils::formatSize(1024LL * 1024 * 10),  "10.0 MB");
    EXPECT_EQ(FormatUtils::formatSize(1024LL * 1024 * 512), "512.0 MB");
}

TEST(FormatUtilsTest, Gigabytes) {
    EXPECT_EQ(FormatUtils::formatSize(1024LL * 1024 * 1024),      "1.0 GB");
    EXPECT_EQ(FormatUtils::formatSize(1024LL * 1024 * 1024 * 5),  "5.0 GB");
}
