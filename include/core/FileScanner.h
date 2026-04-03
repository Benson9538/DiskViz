// #pragma once：防止同一個標頭檔被 #include 兩次
// 因為 #include 本質是複製貼上，重複引入會造成「重複定義」錯誤
#pragma once

#include <filesystem>
#include <vector>
#include <string>

// namespace 別名：std::filesystem 太長，用 fs:: 代替
namespace fs = std::filesystem;

// FileEntry：代表一個被掃描到的檔案
// 用 struct 而不是 class，因為這裡只是純資料容器，不需要封裝行為
struct FileEntry {
    // 完整路徑
    fs::path    path;    
    // 檔案大小（bytes）   
    // 用 uintmax_t 而不是 int，因為 int 最大只有約 2GB    
    uintmax_t   size;       
    // 副檔名（小寫），例如 ".mp4"     
    std::string extension;  
};

// ScanEntry：UI 的展示單位，可能是檔案或資料夾
struct ScanEntry {
    fs::path    path;
    // 檔案：實際大小；資料夾：總大小（計算完之前是 0）
    uintmax_t   totalSize;   
    bool        isDirectory;
    // 資料夾的話是空字串
    std::string extension;   
};

class FileScanner {
public:
    // 加入一個要掃描的根路徑
    void addScanPath(const fs::path& path);

    // 原始遞迴掃描，回傳所有檔案（FileClassifier 使用）
    std::vector<FileEntry> scan();

    // 頂層掃描：根目錄的檔案直接列出，子資料夾以資料夾為單位
    std::vector<ScanEntry> scanTopLevel(const fs::path& root);

    // 深入掃描：對特定資料夾遞迴，回傳個別檔案
    std::vector<ScanEntry> scanDeep(const fs::path& root);

private:
    // 待掃描的路徑清單
    // 後綴底線是 C++ private 變數的命名慣例
    std::vector<fs::path> scanPaths_;  
};