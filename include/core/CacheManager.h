#pragma once

#include <QObject>
#include <QSqlDatabase>
#include <QDateTime>
#include <QString>
#include <vector>
#include "core/FileScanner.h"
#include "core/FileClassifier.h"

struct CachedEntry {
    QString rootPath;
    QString path;
    qint64 size;
    QString category;
    bool isDir;
};

class CacheManager : public QObject
{
    Q_OBJECT
public:
    explicit CacheManager(QObject* parent = nullptr);
    ~CacheManager();
    
    // 初始化資料庫(建立資料表)
    bool init();

    // 儲存掃描結果
    void saveEntries(const QString& rootPath,
                    const std::vector<CachedEntry>& entries);

    std::vector<CachedEntry> loadEntries(const QString& rootPath);

    // 取得上次掃描時間
    QDateTime lastScanTime(const QString& rootPath);

    // 檢查是否有 cache
    bool hasCache(const QString& rootPath);

private:
    QSqlDatabase db_;
    // 建立資料表(如果不存在)
    bool createTable();
};