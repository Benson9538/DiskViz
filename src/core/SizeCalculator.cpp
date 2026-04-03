#include "core/SizeCalculator.h"
#include <iostream>

// ── SizeWorker ────────────────────────────────────────────

SizeWorker::SizeWorker(const std::vector<fs::path>& paths)
    : QObject(nullptr)  // nullptr：沒有 parent，才能 moveToThread
    , paths_(paths)
{}

qint64 SizeWorker::calcDirectorySize(const fs::path& dir)
{
    qint64 total = 0;
    auto options = fs::directory_options::skip_permission_denied;

    try {
        for (const auto& entry :
            fs::recursive_directory_iterator(dir, options))
        {
            if (!entry.is_regular_file()) continue;
            try {
                // static_cast<qint64>：強制轉型
                // file_size() 回傳 uintmax_t，需要轉成 qint64 才能累加
                total += static_cast<qint64>(entry.file_size());
            } catch (const fs::filesystem_error&) {}
        }
    } catch (const fs::filesystem_error& e) {
        std::cerr << "[SizeWorker] 錯誤: " << e.what() << "\n";
    }

    return total;
}

void SizeWorker::calculate()
{
    for (const auto& path : paths_) {
        qint64 size = calcDirectorySize(path);

        // emit：Qt 關鍵字，用來發射 signal
        // 每算完一個就通知 UI，不需要等全部算完
        emit sizeReady(QString::fromStdString(path.string()), size);
    }

    // 全部算完，通知控制者停止執行緒
    emit finished();
}

// ── SizeCalculator ────────────────────────────────────────

SizeCalculator::SizeCalculator(QObject* parent)
    : QObject(parent)
    , thread_(new QThread(this))  // parent 是 this，thread_ 跟著一起釋放
    , worker_(nullptr)
{}

SizeCalculator::~SizeCalculator()
{
    // 確保執行緒正確停止，避免程式關閉時執行緒還在跑
    thread_->quit();
    thread_->wait();
}

void SizeCalculator::addPaths(const std::vector<fs::path>& paths)
{
    paths_ = paths;
}

void SizeCalculator::start()
{
    // 每次 start() 建立新的 Worker（沒有 parent）
    worker_ = new SizeWorker(paths_);

    // moveToThread：把 Worker 移到背景執行緒
    // 之後 Worker 的 slot 都會在背景執行緒上執行
    // ⚠️ moveToThread 之後不能從主執行緒直接呼叫 Worker 的函式
    //    只能透過 signal/slot 溝通
    worker_->moveToThread(thread_);

    // Worker 的 signal 直接轉發給外部
    // 外部只需要跟 SizeCalculator 連接，不需要知道 SizeWorker
    connect(worker_, &SizeWorker::sizeReady,
            this,    &SizeCalculator::sizeReady);
    connect(worker_, &SizeWorker::finished,
            this,    &SizeCalculator::finished);

    // 執行緒啟動時自動呼叫 Worker 的 calculate
    connect(thread_, &QThread::started,
            worker_, &SizeWorker::calculate);

    // 算完後的清理連接鏈：
    // finished → 停止執行緒
    // finished → 安全刪除 Worker
    connect(worker_, &SizeWorker::finished,
            thread_, &QThread::quit);
    // deleteLater：Qt 的安全刪除方式
    // 不能在背景執行緒裡直接 delete 物件
    // deleteLater 會等執行緒回到事件迴圈後才刪除，避免 race condition
    connect(worker_, &SizeWorker::finished,
            worker_, &QObject::deleteLater);


    thread_->start();
}

// moc 檔案必須在 .cpp 最後引入
// 因為標頭檔在 include/core/ 而不是 src/core/
// moc 找不到對應的標頭，需要手動告訴它
#include "moc_SizeCalculator.cpp"