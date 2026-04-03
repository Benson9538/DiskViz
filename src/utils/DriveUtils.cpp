#include "utils/DriveUtils.h"
#include <iostream>

namespace DriveUtils {

std::vector<fs::path> getAvailableDrives()
{
    std::vector<fs::path> drives;

    // WSL 環境下，Windows 的磁碟掛載在 /mnt/ 底下
    // C槽 → /mnt/c，D槽 → /mnt/d，依此類推
    fs::path mntPath = "/mnt";
    if (!fs::exists(mntPath)) return drives;

    for (const auto& entry : fs::directory_iterator(mntPath))
    {
        if (!entry.is_directory()) continue;

        std::string name = entry.path().filename().string();

        // 只取單一字母的目錄（a-z）
        // 排除 wsl、wslg、wsl2 等 WSL 內部特殊目錄
        // length() == 1 且是字母，才是真正的磁碟
        if (name.length() == 1 && std::isalpha(name[0]))
            drives.push_back(entry.path());
    }

    return drives;
}

}