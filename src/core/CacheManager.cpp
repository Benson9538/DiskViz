#include "core/CacheManager.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QStandardPaths>
#include <QDir>
#include <iostream>

using namespace std;

CacheManager::CacheManager(QObject* parent)
    : QObject(parent)
{}

CacheManager::~CacheManager()
{
    if(db_.isOpen()) db_.close();
}

bool CacheManager::init()
{
    // 取得應用程式資料目錄
    // QStandardPaths::AppDataLocation : 跨平台的應用程式資料路徑
    // Linux: ~/.local/share/DiskViz
    // Windows: C:/Users/User/AppData/Roaming/DiskViz
    QString dataDir = QStandardPaths::writableLocation(
        QStandardPaths::AppDataLocation);

    // 確保資料目錄存在
    QDir().mkpath(dataDir);
    QString dbPath = dataDir + "/cache.db";
    cout << "[CacheManager] 資料庫路徑 : " << dbPath.toStdString() << endl;

    return init(dbPath);
}

bool CacheManager::init(const QString& dbPath)
{
    // 避免連線名稱衝突（測試時每個 instance 用不同名稱）
    static int counter = 0;
    QString connName = QString("DiskVizCache_%1").arg(counter++);

    db_ = QSqlDatabase::addDatabase("QSQLITE", connName);
    db_.setDatabaseName(dbPath);

    if(!db_.open()){
        cerr << "[CacheManager] 無法開啟資料庫 : "
            << db_.lastError().text().toStdString() << endl;
        return false;
    };

    return createTable();
}

bool CacheManager::createTable()
{
    QSqlQuery query(db_);

    // 建立掃描紀錄表
    bool ok = query.exec(R"(
        CREATE TABLE IF NOT EXISTS scan_records(
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            root_path TEXT NOT NULL,
            scan_time TEXT NOT NULL
        )
    )");

    if(!ok){
        cerr << "[CacheManager] 建立 scan_records 失敗 :"
            << query.lastError().text().toStdString() << endl;
        return false;
    }

    ok = query.exec(R"(
        CREATE TABLE IF NOT EXISTS scan_entries(
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            record_id INTEGER NOT NULL,
            root_path TEXT NOT NULL,
            path TEXT NOT NULL,
            size INTEGER NOT NULL,
            category TEXT,
            is_dir INTEGER NOT NULL,
            FOREIGN KEY(record_id) REFERENCES scan_records(id)
        )
    )");

    if(!ok){
        cerr << "[CacheManager] 建立 scan_entries 失敗 :"
            << query.lastError().text().toStdString() << endl;
        return false;
    }

    return true;
}

void CacheManager::saveEntries(const QString& rootPath,
                        const vector<CachedEntry>& entries)
{
    QSqlQuery query(db_);
    
    // 用 transaction 包住所有寫入動作
    // transaction : 要嘛全成功，要嘛全退回
    // 避免執行到一半當掉的資料不一致
    // 並大幅提升批次寫入速度
    db_.transaction();

    // 刪除根路徑的舊紀錄
    query.prepare("DELETE FROM scan_entries WHERE root_path = :root");
    query.bindValue(":root", rootPath);
    query.exec();

    query.prepare("DELETE FROM scan_records WHERE root_path = :root");
    query.bindValue(":root", rootPath);
    query.exec();

    // 插入新的掃描紀錄
    // :root , :time : 具名佔位符 -> bindValue 填入實際值
    // 避免 SQL injection
    query.prepare(R"(
        INSERT INTO scan_records (root_path, scan_time)
        VALUES (:root, :time)   
    )");
    query.bindValue(":root", rootPath);
    query.bindValue(":time", 
        QDateTime::currentDateTime().toString(Qt::ISODate));
    query.exec();

    // 取得剛插入的 recoid_id
    // lastInsertId() : 取得最後一次 INSERT 的自動產生的 ID
    qint64 recordId = query.lastInsertId().toLongLong();

    // 批次插入所有項目
    query.prepare(R"(
        INSERT INTO scan_entries
            (record_id, root_path, path, size, category, is_dir)
        VALUES
            (:rid, :root, :path, :size, :cat, :isdir)
    )");

    for(const auto& e : entries){
        query.bindValue(":rid", recordId);
        query.bindValue(":root", rootPath);
        query.bindValue(":path", e.path);
        query.bindValue(":size", e.size);
        query.bindValue(":cat", e.category);
        query.bindValue(":isdir", e.isDir ? 1 : 0);
        query.exec();
    }

    // 提交 transaction
    db_.commit();

    cout << "[CacheManager] 已儲存 " << entries.size()
        << " 筆資料，根路徑 : " << rootPath.toStdString() << endl;
}

vector<CachedEntry> CacheManager::loadEntries(const QString& rootPath)
{
    vector<CachedEntry> res;
    QSqlQuery query(db_);
    query.prepare(R"(
        SELECT path, size, category, is_dir
        FROM scan_entries
        WHERE root_path = :root
        ORDER BY size DESC
    )");
    query.bindValue(":root", rootPath);

    if(!query.exec()){
        cerr << "[CacheManager] 讀取失敗 : " 
            << query.lastError().text().toStdString() << endl;
        return res;
    }

    // 逐列讀取結果
    while(query.next()){
        CachedEntry e;
        e.rootPath = rootPath;
        e.path = query.value(0).toString();
        e.size = query.value(1).toLongLong();
        e.category = query.value(2).toString();
        e.isDir = query.value(3).toInt() ==1;
        res.push_back(e);
    }
    return res;
}

QDateTime CacheManager::lastScanTime(const QString& rootPath)
{
    QSqlQuery query(db_);
    query.prepare(R"(
        SELECT scan_time 
        FROM scan_records
        WHERE root_path = :root
        ORDER BY scan_time DESC
        LIMIT 1
    )");
    query.bindValue(":root", rootPath);
    query.exec();

    if(query.next()) return QDateTime::fromString(
                    query.value(0).toString(), Qt::ISODate);
    
    // 沒有快取紀錄 -> 回傳無效的 QDateTime
    return QDateTime();
}

bool CacheManager::hasCache(const QString& rootPath)
{
    QSqlQuery query(db_);
    query.prepare(R"(
        SELECT COUNT(*) FROM scan_records
        WHERE root_path = :root    
    )");
    query.bindValue(":root", rootPath);
    query.exec();

    if(query.next()) return query.value(0).toInt() > 0;

    return false;
}

void CacheManager::updateSize(const QString& path, qint64 size)
{
    QSqlQuery query(db_);
    query.prepare(R"(
        UPDATE scan_entries
        SET size = :size
        WHERE path = :path
    )");
    query.bindValue(":size", size);
    query.bindValue(":path", path);

    if (!query.exec()) {
        cerr << "[CacheManager] updateSize 失敗: "
             << query.lastError().text().toStdString() << "\n";
    }
}

void CacheManager::updateCategory(const QString& path, const QString& category)
{
    QSqlQuery query(db_);

    // UPDATE 只改一欄，不需要重新寫整筆資料
    query.prepare(R"(
        UPDATE scan_entries
        SET category = :cat
        WHERE path = :path
    )");
    query.bindValue(":cat",  category);
    query.bindValue(":path", path);

    if (!query.exec()) {
        cerr << "[CacheManager] updateCategory 失敗: "
             << query.lastError().text().toStdString() << "\n";
    }
}

#include "moc_CacheManager.cpp"