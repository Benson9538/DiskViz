#pragma once

#include <vector>
#include <filesystem>

namespace fs = std::filesystem;

// DriveUtils：磁碟偵測工具
namespace DriveUtils {

    // 回傳系統上所有可用的磁碟路徑
    // WSL 環境：掃描 /mnt/ 底下的單字母目錄
    //           回傳 /mnt/c、/mnt/d 等
    // 未來 Windows 原生環境可以替換實作
    //           回傳 C:\、D:\ 等
    std::vector<fs::path> getAvailableDrives();

}