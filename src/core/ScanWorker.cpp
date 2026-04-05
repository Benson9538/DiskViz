#include "core/ScanWorker.h"
#include <iostream>

using namespace std;

// ScanWorkerImpl : 實際在背後執行掃描的 worker
ScanWorkerImpl::ScanWorkerImpl(const vector<fs::path>& paths)
    : QObject(nullptr) // 沒有 parent 才能移到 thread
    , paths_(paths) {}

qint64 ScanWorkerImpl::calculateDirSize(const fs::path& path)
{
    qint64 total = 0;
    auto options = fs::directory_options::skip_permission_denied;

    try{
        for(const auto& entry :
            fs::recursive_directory_iterator(path, options))
        {
            if(!entry.is_regular_file()) continue;
            try{
                total += static_cast<qint64>(entry.file_size());
            } catch(const fs::filesystem_error&) {}
        }
    } catch(const fs::filesystem_error& e){
        cerr << "[ScanWorker] 錯誤 :" << e.what() << endl;
    }
    return total;
}

void ScanWorkerImpl::run()
{   
    vector<ScanResult> results;

    for(const auto& path : paths_){
        auto entries = scanner_.scanTopLevel(path);

        for(const auto& e : entries){
            ScanResult r;
            r.rootPath = path;  
            r.path = e.path;
            r.isDirectory = e.isDirectory;
            r.extension = e.extension;
            
            if(e.isDirectory) r.totalSize = calculateDirSize(e.path);
            else r.totalSize = static_cast<qint64>(e.totalSize);

            FileEntry fe {e.path, (uintmax_t)e.totalSize, e.extension};
            r.category = categoryToString(classifier_.classify(fe));

            results.push_back(r);
        } 
    }
    // 結束後發射 finished signal，傳遞結果給 MainWindow
    emit finished(results);
}

// ScanWorker
ScanWorker::ScanWorker(QObject* parent)
    : QObject(parent)
    , thread_(new QThread(this))
    , impl_(nullptr)
{}

ScanWorker::~ScanWorker()
{
    thread_->quit();
    thread_->wait();
}

void ScanWorker::scan(const vector<fs::path>& paths)
{
    // 如果上次的還在跑，停掉
    if(thread_->isRunning()){
        thread_->quit();
        thread_->wait();
    }

    impl_ = new ScanWorkerImpl(paths);
    impl_->moveToThread(thread_);

    
    connect(thread_, &QThread::started,
            impl_, &ScanWorkerImpl::run);
    
    // 結果轉發出去
    connect(impl_, &ScanWorkerImpl::finished,
            this,  &ScanWorker::scanFinished);

    // 清理
    connect(impl_,   &ScanWorkerImpl::finished,
            thread_, &QThread::quit);
    connect(impl_,  &ScanWorkerImpl::finished,
            impl_,  &QObject::deleteLater);

    thread_->start();
}

#include "moc_ScanWorker.cpp"