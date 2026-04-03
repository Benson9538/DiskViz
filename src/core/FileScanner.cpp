#include "core/FileScanner.h"
#include <algorithm>
#include <iostream>

// USER_DIR_BLACKLIST：使用者目錄中不需要顯示的系統資料夾
// static 代表這個變數只屬於這個 .cpp 檔，外部看不到
// const 代表這份清單不會被修改
static const std::vector<std::string> USER_DIR_BLACKLIST = {
    "WindowsPowerShell",  // 系統自動建立的設定資料夾
    "Music",           // Windows 內建捷徑，通常是空的
    "Pictures",        // 同上
    "Videos",          // 同上
    "Games",           // 同上
    "desktop.ini",        // Windows 系統設定檔
};

void FileScanner::addScanPath(const fs::path& path)
{
    scanPaths_.push_back(path);
}

std::vector<FileEntry> FileScanner::scan()
{
    std::vector<FileEntry> results;

    for (const auto& rootPath : scanPaths_)
    {
        // 路徑不存在就跳過，繼續處理下一個
        if (!fs::exists(rootPath)) {
            std::cerr << "[FileScanner] 路徑不存在: " << rootPath << "\n";
            continue;
        }

        // skip_permission_denied：自動跳過沒有權限的資料夾
        // 不加這個選項，碰到系統保護的資料夾會丟出例外讓程式 crash
        auto options = fs::directory_options::skip_permission_denied;

        for (const auto& entry :
             fs::recursive_directory_iterator(rootPath, options))
        {
            // 只處理一般檔案，跳過資料夾、捷徑等
            if (!entry.is_regular_file()) continue;

            // 取得副檔名並轉小寫
            // 轉小寫是為了讓 .MP4 和 .mp4 被視為同一種
            std::string ext = entry.path().extension().string();
            std::transform(ext.begin(), ext.end(),
                           ext.begin(), ::tolower);

            // file_size() 也可能丟出例外（例如符號連結指向已刪除的目標）
            // 用 try/catch 捕捉，遇到就跳過，不讓整個掃描中斷
            uintmax_t size = 0;
            try {
                size = entry.file_size();
            } catch (const fs::filesystem_error& e) {
                std::cerr << "[FileScanner] 無法取得大小: "
                          << e.what() << "\n";
                continue;
            }

            // C++11 聚合初始化：按照 struct 欄位順序填值
            // 等同於先建立 FileEntry f; 再逐一設定欄位
            results.push_back({ entry.path(), size, ext });
        }
    }

    return results;
}

std::vector<ScanEntry> FileScanner::scanTopLevel(const fs::path& root)
{
    std::vector<ScanEntry> results;

    if (!fs::exists(root)) return results;

    auto options = fs::directory_options::skip_permission_denied;

    for (const auto& entry : fs::directory_iterator(root, options))
    {
        // 過濾使用者目錄黑名單
        std::string name = entry.path().filename().string();

        // std::any_of：範圍內是否有任何一個元素滿足條件
        // Lambda [&]：捕獲外部所有變數的參考（這裡捕獲了 name）
        bool blacklisted = std::any_of(
            USER_DIR_BLACKLIST.begin(), USER_DIR_BLACKLIST.end(),
            [&](const std::string& black) { return name == black; }
        );
        if (blacklisted) continue;

        ScanEntry e;
        e.path        = entry.path();
        e.isDirectory = entry.is_directory();
        e.totalSize   = 0;  // 資料夾大小由 SizeCalculator 在背景計算後填入

        if (entry.is_regular_file()) {
            try {
                e.totalSize = entry.file_size();
            } catch (const fs::filesystem_error&) {}

            e.extension = entry.path().extension().string();
            std::transform(e.extension.begin(), e.extension.end(),
                            e.extension.begin(), ::tolower);
        }

        results.push_back(e);
    }

    return results;
}

std::vector<ScanEntry> FileScanner::scanDeep(const fs::path& root)
{
    std::vector<ScanEntry> results;

    if (!fs::exists(root)) return results;

    auto options = fs::directory_options::skip_permission_denied;

    // 深入掃描只回傳檔案，不回傳資料夾
    // 目的是「看清楚這個資料夾裡有什麼檔案」
    for (const auto& entry :
        fs::recursive_directory_iterator(root, options))
    {
        if (!entry.is_regular_file()) continue;

        ScanEntry e;
        e.path        = entry.path();
        e.isDirectory = false;

        try {
            e.totalSize = entry.file_size();
        } catch (const fs::filesystem_error&) {
            e.totalSize = 0;
        }

        e.extension = entry.path().extension().string();
        std::transform(e.extension.begin(), e.extension.end(),
                        e.extension.begin(), ::tolower);

        results.push_back(e);
    }

    return results;
}