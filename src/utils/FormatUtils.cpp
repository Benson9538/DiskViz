#include "utils/FormatUtils.h"
#include <sstream>
#include <iomanip>

namespace FormatUtils {

std::string formatSize(long long bytes)
{
    // constexpr：編譯期就確定的常數，比 const 更嚴格
    // 不需要執行時才計算，效能更好
    constexpr long long KB = 1024;
    constexpr long long MB = 1024 * KB;
    constexpr long long GB = 1024 * MB;

    // ostringstream：字串輸出串流
    // 可以像 cout 一樣用 << 組合字串，最後用 .str() 取出結果
    // 適合需要格式化的字串組合
    std::ostringstream oss;

    // std::fixed：用固定小數點格式，不用科學記號
    // std::setprecision(1)：保留一位小數
    // 例如：1.54321 GB → "1.5 GB"
    oss << std::fixed << std::setprecision(1);

    if (bytes >= GB)
        // static_cast<double>：強制轉成浮點數再除
        // 整數除整數會截掉小數（例如 1622564 / 1048576 = 1）
        // 轉成 double 才能保留小數部分
        oss << static_cast<double>(bytes) / GB << " GB";
    else if (bytes >= MB)
        oss << static_cast<double>(bytes) / MB << " MB";
    else if (bytes >= KB)
        oss << static_cast<double>(bytes) / KB << " KB";
    else
        oss << bytes << " B";

    return oss.str();
}

}