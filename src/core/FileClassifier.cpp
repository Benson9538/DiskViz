#include "core/FileClassifier.h"
#include <algorithm>

// Category 轉中文字串，給 UI 顯示用
std::string categoryToString(Category cat)
{
    switch (cat) {
        case Category::Downloads:   return "下載";
        case Category::Video:       return "影片";
        case Category::Image:       return "圖片";
        case Category::Document:    return "文件";
        case Category::Music:       return "音樂";
        case Category::Archive:     return "壓縮檔";
        case Category::Application: return "應用程式";
        case Category::Code:        return "程式碼";
        case Category::Game:        return "遊戲";
        case Category::Work:        return "工作";
        case Category::Unknown:     return "未知";
        default:                    return "未知";
    }
}

FileClassifier::FileClassifier()
{
    // 用 initializer_list 一次對多個副檔名設定同一個分類
    // 比一行一行寫 extensionMap_[".mp4"] = Category::Video 簡潔很多

    // 影片
    for (const auto& ext : {".mp4", ".mkv", ".avi", ".mov",
                            ".wmv", ".flv", ".webm"})
        extensionMap_[ext] = Category::Video;

    // 圖片
    for (const auto& ext : {".jpg", ".jpeg", ".png", ".gif",
                            ".bmp", ".webp", ".svg", ".psd", ".ai"})
        extensionMap_[ext] = Category::Image;

    // 文件
    for (const auto& ext : {".pdf", ".doc", ".docx", ".xls",
                            ".xlsx", ".ppt", ".pptx", ".txt", ".md"})
        extensionMap_[ext] = Category::Document;

    // 音樂
    for (const auto& ext : {".mp3", ".wav", ".flac", ".aac", ".ogg"})
        extensionMap_[ext] = Category::Music;

    // 壓縮檔
    for (const auto& ext : {".zip", ".rar", ".7z", ".tar", ".gz"})
        extensionMap_[ext] = Category::Archive;

    // 應用程式
    for (const auto& ext : {".exe", ".msi", ".deb", ".appimage"})
        extensionMap_[ext] = Category::Application;

    // 程式碼
    for (const auto& ext : {".cpp", ".h", ".py", ".js", ".ts",
                            ".html", ".css", ".json", ".java", ".cs"})
        extensionMap_[ext] = Category::Code;
}

Category FileClassifier::classifyByPath(const fs::path& path) const
{
    // 把整個路徑轉成小寫字串來比對關鍵字
    // 這樣 Steam 和 steam 都能被識別
    std::string pathStr = path.string();
    std::transform(pathStr.begin(), pathStr.end(),
                    pathStr.begin(), ::tolower);

    // ── Windows 標準資料夾（路徑本身就是分類資訊）──────────
    // 優先順序最高，放在所有其他判斷之前

    if (pathStr.find("downloads") != std::string::npos)
        return Category::Downloads;

    // pictures / screenshots / photos / camera roll
    if (pathStr.find("pictures")    != std::string::npos ||
        pathStr.find("screenshots") != std::string::npos ||
        pathStr.find("photos")      != std::string::npos ||
        pathStr.find("camera roll") != std::string::npos)
        return Category::Image;

    // videos / movies
    if (pathStr.find("videos") != std::string::npos ||
        pathStr.find("movies") != std::string::npos)
        return Category::Video;

    // music
    if (pathStr.find("music") != std::string::npos)
        return Category::Music;

    // documents
    if (pathStr.find("documents") != std::string::npos)
        return Category::Document;

    // ── 遊戲相關路徑關鍵字 ──────────────────────────────
    for (const auto& kw : {"steam", "epic games", "rockstar",
                            "riot", "ubisoft", "gog"})
        if (pathStr.find(kw) != std::string::npos)
            return Category::Game;

    // ── 工作軟體相關路徑關鍵字 ──────────────────────────
    for (const auto& kw : {"office", "adobe", "autodesk",
                            "slack", "zoom", "docker"})
        if (pathStr.find(kw) != std::string::npos)
            return Category::Work;

    return Category::Unknown;
}

Category FileClassifier::classify(const FileEntry& file) const
{
    // 分類優先順序：
    // 1. 路徑規則（Downloads、遊戲、工作關鍵字）
    // 2. 副檔名規則
    // 3. Unknown : Ollama 救救我 QQ

    Category byPath = classifyByPath(file.path);
    if (byPath != Category::Unknown)
        return byPath;

    if (extensionMap_.count(file.extension))
        return extensionMap_.at(file.extension);

    return Category::Unknown;
}