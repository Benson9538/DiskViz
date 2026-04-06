#include "core/SizeCalculator.h"
#include <QThreadPool>
#include <QThread>
#include <iostream>

// ── SizeCalculator ────────────────────────────────────────

SizeCalculator::SizeCalculator(QObject* parent)
    : QObject(parent)
{}

qint64 SizeCalculator::calcDirectorySize(const fs::path& dir)
{
    qint64 total = 0;
    auto options = fs::directory_options::skip_permission_denied;

    try {
        for (const auto& entry :
            fs::recursive_directory_iterator(dir, options))
        {
            if (!entry.is_regular_file()) continue;
            try {
                total += static_cast<qint64>(entry.file_size());
            } catch (const fs::filesystem_error&) {}
        }
    } catch (const fs::filesystem_error& e) {
        std::cerr << "[SizeCalculator] 錯誤: " << e.what() << "\n";
    }

    return total;
}

void SizeCalculator::start()
{
    if (paths_.empty()) {
        emit finished();
        return;
    }

    // 設定最大 thread 數為 CPU 核心數的一半（至少 1）
    int maxThreads = std::max(1, QThread::idealThreadCount() / 2);
    QThreadPool::globalInstance()->setMaxThreadCount(maxThreads);

    pending_ = static_cast<int>(paths_.size());

    for (const auto& path : paths_) {
        QString qPath = QString::fromStdString(path.string());

        // QFutureWatcher 監聽單一 task 的完成事件
        // parent = this，讓它跟著 SizeCalculator 一起釋放
        auto* watcher = new QFutureWatcher<qint64>(this);

        connect(watcher, &QFutureWatcher<qint64>::finished, this,
            [this, watcher, qPath]() {
                emit sizeReady(qPath, watcher->result());
                watcher->deleteLater();

                // 全部算完才通知外部
                if (--pending_ == 0) emit finished();
            });

        // 丟進 thread pool，超過 maxThreads 的 task 自動排隊
        watcher->setFuture(
            QtConcurrent::run([path]() -> qint64 {
                return calcDirectorySize(path);
            })
        );
    }
}

void SizeCalculator::addPaths(const std::vector<fs::path>& paths)
{
    paths_ = paths;
}

#include "moc_SizeCalculator.cpp"
