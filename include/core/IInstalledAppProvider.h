#pragma once

#include <string>
#include <vector>
#include <filesystem>

namespace fs = std::filesystem;

// InstalledApp：代表一個已安裝的應用程式
struct InstalledApp {
    // 軟體名稱
    std::string name;
    // 安裝路徑     
    fs::path    location; 
};

// IInstalledAppProvider：抽象介面
// 定義「取得已安裝軟體清單」這件事的規格，但不提供實作
// 任何繼承這個介面的 class 都必須實作 getInstalledApps()
//
// 設計目的：讓上層程式碼不需要知道底層是用哪種方式取得清單
// 目前：ProgramFilesProvider（掃資料夾）
// 未來：RegistryProvider（讀 Windows Registry）
// 切換時只需要換掉實作，上層完全不用改
class IInstalledAppProvider {
public:
    // pure virtual（純虛擬函式）：= 0 代表只有宣告沒有實作
    // 繼承的 class 一定要實作，否則編譯器直接報錯
    virtual std::vector<InstalledApp> getInstalledApps() = 0;

    // virtual 解構子：繼承體系一定要加
    // 沒有這行的話，用基底類別指標 delete 子類別物件時
    // 只會呼叫基底類別的解構子，子類別資源不會被釋放 → 記憶體洩漏
    virtual ~IInstalledAppProvider() = default;
};