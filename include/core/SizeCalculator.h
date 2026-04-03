#pragma once

#include <QObject>
#include <QString>
#include <QThread>
#include <filesystem>
#include <vector>

namespace fs = std::filesystem;

// ── SizeWorker：實際執行計算的工作者 ────────────────────
// 設計模式：Worker 分離模式
// 把「實際工作」和「控制邏輯」分開，讓 Worker 可以安全移到背景執行緒
//
// 為什麼需要分離？
// Qt 規定：有 parent 的 QObject 不能 moveToThread
// SizeCalculator 有 parent（掛在 MainWindow 下），所以不能直接移動
// 解法：建立一個沒有 parent 的 SizeWorker，把它移到背景執行緒
class SizeWorker : public QObject {
    Q_OBJECT  // 必須加，告訴 Qt moc 處理這個 class 的 signal/slot

public:
    // 沒有 parent 參數，確保可以 moveToThread
    explicit SizeWorker(const std::vector<fs::path>& paths);

public slots:
    // slot：可以被 signal 連接呼叫的函式
    // 執行緒啟動時會自動呼叫這個函式
    void calculate();

signals:
    // signal：只需要宣告，moc 自動產生實作
    // 回傳型別永遠是 void
    void sizeReady(const QString& path, qint64 totalSize);
    void finished();

private:
    std::vector<fs::path> paths_;
    qint64 calcDirectorySize(const fs::path& dir);
};

// ── SizeCalculator：控制者，留在主執行緒 ────────────────
// 負責建立執行緒、管理 Worker 生命週期、轉發 signal
// 外部只需要跟 SizeCalculator 溝通，不需要知道 SizeWorker 的存在
class SizeCalculator : public QObject {
    Q_OBJECT

public:
    explicit SizeCalculator(QObject* parent = nullptr);
    ~SizeCalculator();

    void addPaths(const std::vector<fs::path>& paths);
    void start();

signals:
    // 轉發 SizeWorker 的 signal 給外部
    void sizeReady(const QString& path, qint64 totalSize);
    void finished();

private:
    std::vector<fs::path> paths_;
    QThread*    thread_;  // 背景執行緒
    SizeWorker* worker_;  // 實際工作者
};