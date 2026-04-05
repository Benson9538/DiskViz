#pragma once

#include <QMainWindow>
#include <QTreeWidget>    // 樹狀清單元件，支援多層展開
#include <QListWidget>    // 清單元件，用於類別選單和掃描路徑勾選
#include <QPushButton>
#include <QLabel>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QWidget>
#include <QSplitter>      // 可拖動的左右分割器

#include "core/FileScanner.h"
#include "core/FileClassifier.h"
#include "core/SizeCalculator.h"
#include "core/CacheManager.h"
#include "core/ScanWorker.h"
#include "utils/FormatUtils.h"
#include "utils/DriveUtils.h"

class MainWindow : public QMainWindow {
    Q_OBJECT  // 必須加，讓 moc 處理 signal/slot

public:
    // explicit：防止編譯器做隱式型別轉換
    // parent = nullptr：預設沒有父物件
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow();

// slots：可以被 signal 連接呼叫的函式
private slots:
    void onScanClicked();
    void onSizeReady(const QString& path, qint64 size);
    void onCategoryChanged(int row);
    void onItemExpanded(QTreeWidgetItem* item);
    void onScanFinished(const std::vector<ScanResult>& results);

private:
    // UI 元件 
    QWidget*     sidebar_;        // 左側導覽列容器
    QListWidget* scanPathList_;   // 掃描路徑勾選清單
    QListWidget* categoryList_;   // 類別篩選清單
    QSplitter*   splitter_;       // 左右分割器
    QTreeWidget* contentTree_;    // 右側內容樹狀清單
    QPushButton* scanButton_;     // 掃描按鈕
    QLabel*      statusLabel_;    // 底部狀態文字

    // 核心模組
    FileScanner     scanner_;      // 檔案掃描器（值，不是指標）
    FileClassifier  classifier_;   // 分類引擎（值，不是指標）
    SizeCalculator* sizeCalculator_; // 大小計算器
    // SizeCalculator 繼承 QObject，QObject 不能複製，只能用指標
    CacheManager*   cacheManager_; // 快取管理器
    ScanWorker*     scanWorker_;   // 掃描工作執行緒

    // 函式
    void setupUI();
    void setupSidebar();
    void setupContentArea();
    void populateTree(const std::vector<ScanEntry>& entries);
    void loadFromCacheAndScan();
    void populateTreeWithGroups(const std::vector<ScanResult>& results);

    // 取得目前勾選的掃描路徑清單
    std::vector<fs::path> getSelectedScanPaths();


    // 遞迴搜尋整棵樹，找到路徑對應的項目
    // 用於 onSizeReady 更新任意層級的大小顯示
    QTreeWidgetItem* findItemByPath(const QString& path);
};