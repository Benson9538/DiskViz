#pragma once

#include <string>

// FormatUtils：格式化工具函式的命名空間
// 用 namespace 而不是 class，因為這裡只有純粹的工具函式
// 不需要建立物件，直接呼叫 FormatUtils::formatSize() 即可
namespace FormatUtils {

    // 把 bytes 轉成人類可讀的格式
    // 622564     → "1.5 MB"
    // 49         → "49 B"
    // 3000000000 → "2.8 GB"
    std::string formatSize(long long bytes);

}