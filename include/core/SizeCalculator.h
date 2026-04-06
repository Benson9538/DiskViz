#pragma once

#include <QObject>
#include <QString>
#include <QFutureWatcher>
#include <QtConcurrent>
#include <filesystem>
#include <vector>
#include <atomic>

namespace fs = std::filesystem;

// ── SizeCalculator ────────────────────────────────────────
// 使用 QThreadPool + QtConcurrent 平行計算多個目錄大小
// 最大 thread 數 = CPU 核心數 / 2，超過的 task 自動排隊
class SizeCalculator : public QObject {
    Q_OBJECT

public:
    explicit SizeCalculator(QObject* parent = nullptr);

    void addPaths(const std::vector<fs::path>& paths);
    void start();

signals:
    void sizeReady(const QString& path, qint64 totalSize);
    void finished();

private:
    std::vector<fs::path> paths_;
    std::atomic<int> pending_{0};   // 還沒算完的目錄數量

    static qint64 calcDirectorySize(const fs::path& dir);
};
