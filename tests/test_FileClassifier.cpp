#include <gtest/gtest.h>
#include "core/FileClassifier.h"

class FileClassifierTest : public ::testing::Test {
protected:
    FileClassifier classifier;
};

// ── categoryToString ──────────────────────────────────────

TEST_F(FileClassifierTest, CategoryToString) {
    EXPECT_EQ(categoryToString(Category::Video),       "影片");
    EXPECT_EQ(categoryToString(Category::Image),       "圖片");
    EXPECT_EQ(categoryToString(Category::Document),    "文件");
    EXPECT_EQ(categoryToString(Category::Music),       "音樂");
    EXPECT_EQ(categoryToString(Category::Archive),     "壓縮檔");
    EXPECT_EQ(categoryToString(Category::Application), "應用程式");
    EXPECT_EQ(categoryToString(Category::Code),        "程式碼");
    EXPECT_EQ(categoryToString(Category::Game),        "遊戲");
    EXPECT_EQ(categoryToString(Category::Work),        "工作");
    EXPECT_EQ(categoryToString(Category::Unknown),     "未知");
    EXPECT_EQ(categoryToString(Category::Downloads),   "下載");
}

// ── 副檔名分類 ────────────────────────────────────────────

TEST_F(FileClassifierTest, ClassifyByExtension_Video) {
    FileEntry f{fs::path("/test/movie.mp4"), 0, ".mp4"};
    EXPECT_EQ(classifier.classify(f), Category::Video);

    FileEntry f2{fs::path("/test/clip.mkv"), 0, ".mkv"};
    EXPECT_EQ(classifier.classify(f2), Category::Video);
}

TEST_F(FileClassifierTest, ClassifyByExtension_Image) {
    FileEntry f{fs::path("/test/photo.jpg"), 0, ".jpg"};
    EXPECT_EQ(classifier.classify(f), Category::Image);

    FileEntry f2{fs::path("/test/icon.png"), 0, ".png"};
    EXPECT_EQ(classifier.classify(f2), Category::Image);
}

TEST_F(FileClassifierTest, ClassifyByExtension_Document) {
    FileEntry f{fs::path("/test/report.pdf"), 0, ".pdf"};
    EXPECT_EQ(classifier.classify(f), Category::Document);

    FileEntry f2{fs::path("/test/notes.docx"), 0, ".docx"};
    EXPECT_EQ(classifier.classify(f2), Category::Document);
}

TEST_F(FileClassifierTest, ClassifyByExtension_Music) {
    FileEntry f{fs::path("/test/song.mp3"), 0, ".mp3"};
    EXPECT_EQ(classifier.classify(f), Category::Music);

    FileEntry f2{fs::path("/test/track.flac"), 0, ".flac"};
    EXPECT_EQ(classifier.classify(f2), Category::Music);
}

TEST_F(FileClassifierTest, ClassifyByExtension_Archive) {
    FileEntry f{fs::path("/test/backup.zip"), 0, ".zip"};
    EXPECT_EQ(classifier.classify(f), Category::Archive);

    FileEntry f2{fs::path("/test/data.7z"), 0, ".7z"};
    EXPECT_EQ(classifier.classify(f2), Category::Archive);
}

TEST_F(FileClassifierTest, ClassifyByExtension_Application) {
    FileEntry f{fs::path("/test/setup.exe"), 0, ".exe"};
    EXPECT_EQ(classifier.classify(f), Category::Application);
}

TEST_F(FileClassifierTest, ClassifyByExtension_Code) {
    FileEntry f{fs::path("/test/main.cpp"), 0, ".cpp"};
    EXPECT_EQ(classifier.classify(f), Category::Code);

    FileEntry f2{fs::path("/test/script.py"), 0, ".py"};
    EXPECT_EQ(classifier.classify(f2), Category::Code);
}

TEST_F(FileClassifierTest, ClassifyByExtension_Unknown) {
    FileEntry f{fs::path("/test/file.xyz"), 0, ".xyz"};
    EXPECT_EQ(classifier.classify(f), Category::Unknown);
}

// ── 路徑關鍵字分類 ────────────────────────────────────────

TEST_F(FileClassifierTest, ClassifyByPath_Downloads) {
    FileEntry f{fs::path("/Users/User/Downloads/something"), 0, ""};
    EXPECT_EQ(classifier.classify(f), Category::Downloads);
}

TEST_F(FileClassifierTest, ClassifyByPath_Pictures) {
    FileEntry f{fs::path("/Users/User/Pictures/vacation"), 0, ""};
    EXPECT_EQ(classifier.classify(f), Category::Image);

    // Screenshots 也應該分類為圖片
    FileEntry f2{fs::path("/Users/User/Screenshots/screen1"), 0, ""};
    EXPECT_EQ(classifier.classify(f2), Category::Image);
}

TEST_F(FileClassifierTest, ClassifyByPath_PhotoshopIsWork) {
    // "photoshop" 包含 "photos" 子字串，不應該被誤判為圖片
    FileEntry f{fs::path("/Program Files/Adobe/Photoshop"), 0, ""};
    EXPECT_EQ(classifier.classify(f), Category::Work);
}

TEST_F(FileClassifierTest, ClassifyByPath_Videos) {
    FileEntry f{fs::path("/Users/User/Videos/movies"), 0, ""};
    EXPECT_EQ(classifier.classify(f), Category::Video);
}

TEST_F(FileClassifierTest, ClassifyByPath_Music) {
    FileEntry f{fs::path("/Users/User/Music/playlist"), 0, ""};
    EXPECT_EQ(classifier.classify(f), Category::Music);
}

TEST_F(FileClassifierTest, ClassifyByPath_Documents) {
    FileEntry f{fs::path("/Users/User/Documents/report"), 0, ""};
    EXPECT_EQ(classifier.classify(f), Category::Document);
}

TEST_F(FileClassifierTest, ClassifyByPath_Game) {
    FileEntry f{fs::path("/Program Files/Steam/steamapps"), 0, ""};
    EXPECT_EQ(classifier.classify(f), Category::Game);
}

TEST_F(FileClassifierTest, ClassifyByPath_Work) {
    FileEntry f{fs::path("/Program Files/Adobe/Photoshop"), 0, ""};
    EXPECT_EQ(classifier.classify(f), Category::Work);
}

// ── 路徑優先於副檔名 ──────────────────────────────────────

TEST_F(FileClassifierTest, PathTakesPriorityOverExtension) {
    // Downloads 資料夾內的 .mp4，應分類為下載而非影片
    FileEntry f{fs::path("/Users/User/Downloads/video.mp4"), 0, ".mp4"};
    EXPECT_EQ(classifier.classify(f), Category::Downloads);
}
