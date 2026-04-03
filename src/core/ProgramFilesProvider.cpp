#include "core/ProgramFilesProvider.h"
#include <algorithm>

// SYSTEM_FOLDERS：已知的系統資料夾黑名單
// static：只屬於這個 .cpp 檔，外部看不到，避免污染全域命名空間
// const：這份清單不會被修改
static const std::vector<std::string> SYSTEM_FOLDERS = {
    "Common Files",              
    "Windows NT",                 
    "Windows Mail",               
    "Windows Media Player",       
    "Windows Multimedia Platform",
    "Windows Photo Viewer",       
    "WindowsApps",                
    "WindowsPowerShell",          
    "Internet Explorer",          
    "Microsoft Update Health Tools",
    "ModifiableWindowsApps",      
    "MSBuild",                    
    "Reference Assemblies",        
    "Uninstall Information",      
    "NSIS Uninstall Information",    
    "Windows Defender",           
    "Windows Sidebar",            
    "Windows Kits",               
    "WSL",                        
    "Microsoft.NET",               
};

bool ProgramFilesProvider::isSystemFolder(const std::string& name) const
{
    for (const auto& sys : SYSTEM_FOLDERS)
        if (sys == name) return true;
    return false;
}

std::vector<InstalledApp> ProgramFilesProvider::getInstalledApps()
{
    std::vector<InstalledApp> results;

    std::vector<fs::path> programDirs = {
        "/mnt/c/Program Files",
        "/mnt/c/Program Files (x86)"
    };

    for (const auto& dir : programDirs) {
        if (!fs::exists(dir)) continue;

        // 只掃第一層子資料夾，每個資料夾視為一個已安裝軟體
        // 用 directory_iterator 而不是 recursive_directory_iterator
        // 因為我們只需要第一層的資料夾名稱
        for (const auto& entry : fs::directory_iterator(dir)) {
            if (!entry.is_directory()) continue;

            // filename()：取得路徑最後一段，例如 "Steam"
            // string()：轉成 std::string 才能跟黑名單比對
            std::string folderName = entry.path().filename().string();

            if (isSystemFolder(folderName)) continue;

            results.push_back({ folderName, entry.path() });
        }
    }

    return results;
}