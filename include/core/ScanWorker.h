#pragma once

#include <QObject>
#include <QThread>
#include <vector>
#include <filesystem>
#include "core/FileScanner.h"
#include "core/FileClassifier.h"
#include "core/CacheManager.h"

namespace fs = std::filesystem;

struct ScanResult {
    fs::path path;
    fs::path rootPath;
    qint64 totalSize;
    bool isDirectory;
    std::string extension;
    std::string category;
};

// 實際執行掃描的 worker
class ScanWorkerImpl : public QObject {
    Q_OBJECT
public:
    explicit ScanWorkerImpl(const std::vector<fs::path>& paths);

public slots:
    void run();

signals:
    void finished(const std::vector<ScanResult>& results);

private:
    std::vector<fs::path> paths_;
    FileScanner scanner_;
    FileClassifier classifier_;

    qint64 calculateDirSize(const fs::path& path);
};

// 控制者
class ScanWorker : public QObject{
    Q_OBJECT
public:
    explicit ScanWorker(QObject* parent = nullptr);
    ~ScanWorker();

    void scan(const std::vector<fs::path>& paths);

signals:
    void scanFinished(const std::vector<ScanResult>& results);

private:
    QThread* thread_;
    ScanWorkerImpl* impl_;
};