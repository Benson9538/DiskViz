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

    // 更新單筆項目的分類（Ollama 分類完成後呼叫）
    void updateCategory(const QString& path, const QString& category);

    // 更新單筆項目的大小（SizeCalculator 算完後呼叫）
    void updateSize(const QString& path, qint64 size);

private:
    QSqlDatabase db_;
    // 建立資料表(如果不存在)
    bool createTable();
};