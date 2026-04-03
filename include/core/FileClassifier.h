#pragma once

#include "core/FileScanner.h"
#include <string>
#include <unordered_map>

// Category：檔案分類標籤
// enum class（強型別列舉）：C++11 特性
// 使用時必須寫 Category::Video，不會跟其他 enum 的值名稱衝突
// 比舊式 enum 更安全，避免命名污染
enum class Category {
    Downloads,    // Downloads 資料夾內的所有東西
    Video,        // 影片
    Image,        // 圖片
    Document,     // 文件
    Music,        // 音樂
    Archive,      // 壓縮檔
    Application,  // 應用程式
    Code,         // 程式碼
    Game,         // 遊戲（路徑關鍵字判斷）
    Work,         // 工作軟體（路徑關鍵字判斷）
    Unknown       // 以上都不符合，Ollama 救救我 QQ
};

// 把 Category 轉成中文字串，方便 UI 顯示
std::string categoryToString(Category cat);

class FileClassifier {
public:
    // 建構子：初始化副檔名對應表
    FileClassifier();

    // 對單一檔案進行分類，回傳對應的 Category
    // const：這個函式不會修改 class 的任何成員變數
    Category classify(const FileEntry& file) const;

private:
    // 副檔名對應分類的查找表
    // unordered_map：用雜湊表實作的鍵值對，查找速度 O(1)
    // 鍵：副檔名（例如 ".mp4"）
    // 值：對應的 Category
    std::unordered_map<std::string, Category> extensionMap_;

    // 根據路徑關鍵字判斷分類
    Category classifyByPath(const fs::path& path) const;
};
