#pragma once

#include "core/IInstalledAppProvider.h"

// ProgramFilesProvider：IInstalledAppProvider 的實作
// 透過掃描 Program Files 第一層資料夾來取得已安裝軟體清單
// 用黑名單過濾掉已知的系統資料夾
class ProgramFilesProvider : public IInstalledAppProvider {
public:
    // override：明確告訴編譯器這是覆寫父類別的函式
    // 如果函式簽名打錯，編譯器會報錯
    // 不加的話編譯器不會警告，只是默默建立新函式，介面沒被實作
    std::vector<InstalledApp> getInstalledApps() override;

private:
    // 輔助函式：判斷資料夾名稱是否在系統黑名單中
    // const 在函式後面：這個函式不會修改 class 的任何成員變數
    bool isSystemFolder(const std::string& folderName) const;
};