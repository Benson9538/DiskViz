// Qt 程式的入口點
// 執行流程：main() -> QApplication -> MainWindow -> 事件迴圈 -> 關閉後 main() 結束

#include <QApplication>
#include "ui/MainWindow.h"

int main(int argc, char* argv[])
{
    // QApplication 必須是第一個建立的 Qt 物件
    // 負責管理整個程式生命週期（滑鼠、鍵盤、視窗事件等）
    QApplication app(argc, argv);

    MainWindow window;
    window.show();

    // 進入事件迴圈
    // 程式會在這裡等待使用者操作，直到視窗關閉才返回
    return app.exec();
}
