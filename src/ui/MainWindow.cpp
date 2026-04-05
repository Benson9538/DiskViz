#include "ui/MainWindow.h"
#include <QHeaderView>
#include <iostream>

using namespace std;

class SortableTreeItem : public QTreeWidgetItem {
public:
    using QTreeWidgetItem::QTreeWidgetItem;

    bool operator<(const QTreeWidgetItem& other) const override {
        int col = treeWidget()->sortColumn();

        // 大小欄(1) 以數值排序
        if(col == 1){
            qint64 x = data(1, Qt::UserRole).value<qint64>();
            qint64 y = other.data(1, Qt::UserRole).value<qint64>();
            return x < y;
        }
        return QTreeWidgetItem::operator<(other);
    }
};

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , sizeCalculator_(new SizeCalculator(this))
    , cacheManager_(new CacheManager(this))
    , scanWorker_(new ScanWorker(this))
{
    setWindowTitle("DiskViz");
    resize(1280, 800);
    setupUI();

    connect(scanWorker_, &ScanWorker::scanFinished,
            this,        &MainWindow::onScanFinished);

    if(!cacheManager_->init()){
        statusLabel_->setText("快取初始化失敗");
        return;
    }
    loadFromCacheAndScan();
}

MainWindow::~MainWindow() {}

void MainWindow::loadFromCacheAndScan()
{
    auto scanPaths = getSelectedScanPaths();
    if(scanPaths.empty()) return ;

    bool hasAnyCache = false;
    QDateTime oldestScanTime;

    // 先讀快取，立即顯示
    vector<ScanResult> cachedResults;

    for(const auto& path : scanPaths){
        QString qPath = QString::fromStdString(path.string());
        if(cacheManager_->hasCache(qPath)){
            hasAnyCache = true;
            QDateTime t = cacheManager_->lastScanTime(qPath);
            if(!oldestScanTime.isValid() || t < oldestScanTime) oldestScanTime = t;

            auto cached = cacheManager_->loadEntries(qPath);
            for(const auto& e : cached){
                ScanResult r;
                r.rootPath = fs::path(e.rootPath.toStdString());
                r.path = fs::path(e.path.toStdString());
                r.totalSize = e.size;
                r.isDirectory = e.isDir;
                r.extension = fs::path(e.path.toStdString()).extension().string();
                r.category = e.category.toStdString();
                cachedResults.push_back(r);
            }
        }
    }

    if(hasAnyCache){
        populateTreeWithGroups(cachedResults);
        onCategoryChanged(categoryList_->currentRow());

        QString timeStr = oldestScanTime.toString("yyyy/MM/dd hh:mm:ss");
        statusLabel_->setText("上次更新 : " + timeStr + " ，更新中...");
    }
    else statusLabel_->setText("首次掃描中");

    scanButton_->setEnabled(false);
    scanWorker_->scan(scanPaths);
}

void MainWindow::onScanFinished(const vector<ScanResult>& results)
{
    // 更新畫面
    contentTree_->clear();
    populateTreeWithGroups(results);
    onCategoryChanged(categoryList_->currentRow());

    auto scanPaths = getSelectedScanPaths();
    for(const auto& path : scanPaths){
        QString qPath = QString::fromStdString(path.string());

        vector<CachedEntry> toCache;
        for(const auto& r : results){
            if(r.path.string().find(path.string()) != 0) continue;

            CachedEntry ce;
            ce.rootPath = qPath;
            ce.path = QString::fromStdString(r.path.string());
            ce.size = r.totalSize;
            ce.category = QString::fromStdString(r.category);
            ce.isDir = r.isDirectory;
            toCache.push_back(ce);
        }

        if(!toCache.empty()){
            cacheManager_->saveEntries(qPath, toCache);
        }
    }

    QString timeStr = QDateTime::currentDateTime()
                        .toString("yyyy/MM/dd hh:mm:ss");
    statusLabel_->setText("已更新 :" + timeStr);
    scanButton_->setEnabled(true);
}

void MainWindow::setupUI()
{
    // QSplitter：把左右兩個元件包起來
    // 使用者可以拖動中間的分隔線調整比例
    splitter_ = new QSplitter(Qt::Horizontal, this);
    setCentralWidget(splitter_);

    setupSidebar();
    setupContentArea();

    // setStretchFactor(index, factor)：設定初始比例
    // 左側 1：右側 4
    splitter_->setStretchFactor(0, 1);
    splitter_->setStretchFactor(1, 4);
}

void MainWindow::setupSidebar()
{
    sidebar_ = new QWidget(splitter_);
    QVBoxLayout* layout = new QVBoxLayout(sidebar_);
    layout->setContentsMargins(8, 8, 8, 8);

    // 應用程式名稱標題
    QLabel* appName = new QLabel("DiskViz");
    appName->setStyleSheet("font-size: 20px; font-weight: bold;");
    layout->addWidget(appName);

    // 掃描位置區塊
    QLabel* scanTitle = new QLabel("掃描位置");
    scanTitle->setStyleSheet("font-weight: bold; margin-top: 8px;");
    layout->addWidget(scanTitle);

    scanPathList_ = new QListWidget();
    scanPathList_->setMaximumHeight(200);

    // 取得目前 Windows 使用者的家目錄
    string user = "User";
    fs::path userHome = fs::path("/mnt/c/Users") / user;

    // 預設掃描路徑清單（標籤, 路徑）
    // pair：把兩個值打包在一起
    vector<pair<string, fs::path>> defaultPaths = {
        { "Documents",          userHome / "Documents"           },
        { "Desktop",            userHome / "Desktop"             },
        { "Downloads",          userHome / "Downloads"           },
        { "Pictures",           userHome / "Pictures"            },
        { "Videos",             userHome / "Videos"              },
        { "Music",              userHome / "Music"               },
        { "AppData Roaming",    userHome / "AppData" / "Roaming" },
        { "Program Files",      fs::path("/mnt/c/Program Files")       },
        { "Program Files x86",  fs::path("/mnt/c/Program Files (x86)") },
    };

    // C++17 結構化綁定：直接把 pair 解包成有意義的名字
    // 比用 .first 和 .second 更清楚
    for (const auto& [label, path] : defaultPaths) {
        if (!fs::exists(path)) continue;

        QListWidgetItem* item = new QListWidgetItem(
            QString::fromStdString(label), scanPathList_);

        // setCheckState：讓項目顯示勾選框
        item->setCheckState(Qt::Checked);  // 預設全勾選

        // setData(Qt::UserRole, ...)：附帶額外資料
        // Qt::UserRole 是給開發者自訂用途的欄位
        // 我們用它儲存完整路徑，之後取用時不需要自己維護對應表
        item->setData(Qt::UserRole,
            QString::fromStdString(path.string()));
    }

    // 動態偵測其他磁碟（D槽、E槽等）
    auto drives = DriveUtils::getAvailableDrives();
    for (const auto& drive : drives) {
        string driveName = drive.filename().string();
        if (driveName == "c") continue;  // C槽已用上面的方式處理

        string label = driveName + " 槽";
        transform(label.begin(), label.end(),
                        label.begin(), ::toupper);

        QListWidgetItem* item = new QListWidgetItem(
            QString::fromStdString(label), scanPathList_);
        // 其他磁碟預設不勾選
        item->setCheckState(Qt::Unchecked);  
        item->setData(Qt::UserRole,
            QString::fromStdString(drive.string()));
    }

    layout->addWidget(scanPathList_);

    // 掃描按鈕
    scanButton_ = new QPushButton("開始掃描");
    layout->addWidget(scanButton_);

    // 類別篩選區塊
    QLabel* catTitle = new QLabel("類別");
    catTitle->setStyleSheet("font-weight: bold; margin-top: 12px;");
    layout->addWidget(catTitle);

    categoryList_ = new QListWidget();
    categoryList_->addItems({
        "全部", "遊戲", "工作", "影片", "圖片",
        "文件", "音樂", "壓縮檔", "應用程式",
        "程式碼", "下載", "未知"
    });
    categoryList_->setCurrentRow(0);
    layout->addWidget(categoryList_);

    // 狀態文字（底部）
    statusLabel_ = new QLabel("尚未掃描");
    statusLabel_->setStyleSheet("color: gray; margin-top: 12px;");
    layout->addWidget(statusLabel_);

    // Signal/Slot 連接
    // connect(發射者, signal, 接收者, slot)
    connect(scanButton_,   &QPushButton::clicked,
            this,          &MainWindow::onScanClicked);
    connect(categoryList_, &QListWidget::currentRowChanged,
            this,          &MainWindow::onCategoryChanged);
}

void MainWindow::setupContentArea()
{
    QWidget* contentArea = new QWidget(splitter_);
    QVBoxLayout* layout = new QVBoxLayout(contentArea);
    layout->setContentsMargins(8, 8, 8, 8);

    QLabel* title = new QLabel("使用者內容");
    title->setStyleSheet("font-size: 16px; font-weight: bold;");
    layout->addWidget(title);

    // QTreeWidget：樹狀清單，支援多層展開
    contentTree_ = new QTreeWidget();
    contentTree_->setColumnCount(4);
    contentTree_->setHeaderLabels({"名稱", "大小", "類別", "路徑"});

    // 設定欄位寬度
    contentTree_->setColumnWidth(0, 250);  // 名稱
    contentTree_->setColumnWidth(1, 100);  // 大小
    contentTree_->setColumnWidth(2, 80);   // 類別
    // Stretch：路徑欄自動填滿剩餘空間
    contentTree_->header()->setSectionResizeMode(
        3, QHeaderView::Stretch);

    // 點擊標題排序
    contentTree_->setSortingEnabled(true);
    contentTree_->sortByColumn(1, Qt::DescendingOrder);

    layout->addWidget(contentTree_);

    // 使用者展開資料夾時觸發
    connect(contentTree_, &QTreeWidget::itemExpanded,
            this,         &MainWindow::onItemExpanded);
}

vector<fs::path> MainWindow::getSelectedScanPaths()
{
    vector<fs::path> paths;

    for (int i = 0; i < scanPathList_->count(); ++i) {
        QListWidgetItem* item = scanPathList_->item(i);

        // 只取勾選的項目
        if (item->checkState() == Qt::Checked) {
            // data(Qt::UserRole)：取出我們存進去的路徑
            // toString()：轉成 QString
            // toStdString()：轉成 string
            paths.push_back(fs::path(
                item->data(Qt::UserRole).toString().toStdString()
            ));
        }
    }

    return paths;
}

void MainWindow::onScanClicked()
{
    statusLabel_->setText("掃描中...");
    scanButton_->setEnabled(false);

    auto scanPaths = getSelectedScanPaths();

    // 沒有選任何路徑就提示使用者
    if (scanPaths.empty()) {
        statusLabel_->setText("請至少選擇一個掃描位置");
        scanButton_->setEnabled(true);
        return;
    }

    scanWorker_->scan(scanPaths);
}

void MainWindow::populateTree(const vector<ScanEntry>& entries)
{
    // 依大小排序（大的在上面）
    // Lambda 比較函式：a.totalSize > b.totalSize 代表從大到小
    auto sorted = entries;
    sort(sorted.begin(), sorted.end(),
        [](const ScanEntry& a, const ScanEntry& b) {
            return a.totalSize > b.totalSize;
        }
    );

    for (const auto& e : sorted)
    {
        // 把 ScanEntry 轉成 FileEntry 給 classifier 使用
        FileEntry fe{ e.path, e.totalSize, e.extension };
        Category cat = classifier_.classify(fe);

        // new QTreeWidgetItem(contentTree_)：加入頂層項目
        // parent 是 contentTree_ 代表這是頂層，不是子節點
        QTreeWidgetItem* item = new SortableTreeItem(contentTree_);
        item->setText(0, QString::fromStdString(
            e.path.filename().string()));
        
        if(e.isDirectory){
            item->setText(1, "計算中...");
            item->setData(1, Qt::UserRole, QVariant::fromValue((qint64)0));
        }
        else{
            item->setText(1,
                QString::fromStdString(FormatUtils::formatSize(e.totalSize)));
            // QVariant : Qt 的通用資料類型，可以存放各種型別
            item->setData(1, Qt::UserRole,
                QVariant::fromValue((qint64)e.totalSize));
        }

        item->setText(2, QString::fromStdString(categoryToString(cat)));
        item->setText(3, QString::fromStdString(e.path.string()));

        if (e.isDirectory) {
            // ShowIndicator：強制顯示展開箭頭
            // 即使還沒有子項目也要顯示，讓使用者知道可以展開
            item->setChildIndicatorPolicy(
                QTreeWidgetItem::ShowIndicator);
        }
    }
}

void MainWindow::populateTreeWithGroups(
    const vector<ScanResult>& results)
{
    vector<fs::path> rootPaths;
    for(const auto& r : results){
        if(std::find(rootPaths.begin(), rootPaths.end(), 
                r.rootPath) == rootPaths.end())
            rootPaths.push_back(r.rootPath);
    }

    for(const auto& root : rootPaths){
        QTreeWidgetItem* header = new QTreeWidgetItem(contentTree_);
        header->setExpanded(true);
        header->setText(0, QString::fromStdString(
            root.filename().string()));
        // 橫跨所有欄位
        header->setFirstColumnSpanned(true); // 橫跨所有欄位
        header->setBackground(0, QColor(230, 230, 230)); // 灰色
        header->setFlags(header->flags() & ~Qt::ItemIsSelectable); // 不可選取
        QFont font;
        font.setBold(true);
        header->setFont(0, font);

        vector<ScanResult> group;
        for(const auto& r : results){
            if(r.rootPath == root) group.push_back(r);
        }
        sort(group.begin(), group.end(),
            [](const ScanResult& a, const ScanResult& b) {
                return a.totalSize > b.totalSize;
        });
        
        for(const auto& r : group){
            FileEntry fe{r.path, (uintmax_t)r.totalSize, r.extension};
            Category cat = classifier_.classify(fe);

            QTreeWidgetItem* item = new SortableTreeItem(header);
            item->setText(0, QString::fromStdString(
                r.path.filename().string()));
            
            if(r.isDirectory){
                item->setText(1, QString::fromStdString(
                    FormatUtils::formatSize(r.totalSize)));
                item->setData(1, Qt::UserRole, 
                    QVariant::fromValue(r.totalSize));
                item->setChildIndicatorPolicy(
                    QTreeWidgetItem::ShowIndicator);
            }
            else {
                item->setText(1, QString::fromStdString(
                    FormatUtils::formatSize(r.totalSize)));
                item->setData(1, Qt::UserRole, 
                    QVariant::fromValue(r.totalSize));
            }
            item->setText(2, QString::fromStdString(
                categoryToString(cat)));
            item->setText(3, QString::fromStdString(
                r.path.string()));
        }
    }   
}

void MainWindow::onItemExpanded(QTreeWidgetItem* item)
{
    // 已經載入過就不重複載入
    // 避免使用者重複展開收合時，子項目被重複加入
    if (item->childCount() > 0) return;

    // 取得完整路徑（存在第 3 欄）
    fs::path dirPath = item->text(3).toStdString();

    // 用 scanTopLevel 而不是 scanDeep
    // scanTopLevel：看到下一層的資料夾和檔案（適合樹狀展開）
    // scanDeep：遞迴取得所有檔案（適合完整清單）
    auto entries = scanner_.scanTopLevel(dirPath);

    // 依大小排序
    sort(entries.begin(), entries.end(),
        [](const ScanEntry& a, const ScanEntry& b) {
            return a.totalSize > b.totalSize;
        }
    );

    for (const auto& e : entries)
    {
        FileEntry fe{ e.path, e.totalSize, e.extension };
        Category cat = classifier_.classify(fe);

        // new QTreeWidgetItem(item)：parent 是 item
        // 代表這是 item 的子節點，不是頂層項目
        QTreeWidgetItem* child = new SortableTreeItem(item);
        child->setText(0, QString::fromStdString(
            e.path.filename().string()));
        
        if(e.isDirectory){
            child->setText(1, "計算中...");
            child->setData(1, Qt::UserRole, QVariant::fromValue((qint64)0));
        }
        else{
            child->setText(1, QString::fromStdString(FormatUtils::formatSize(e.totalSize)));
            child->setData(1, Qt::UserRole, QVariant::fromValue((qint64)e.totalSize));
        }

        child->setText(2, QString::fromStdString(categoryToString(cat)));
        child->setText(3, QString::fromStdString(e.path.string()));

        if (e.isDirectory) {
            child->setChildIndicatorPolicy(
                QTreeWidgetItem::ShowIndicator);
        }
    }

    // 收集子資料夾，啟動背景大小計算
    vector<fs::path> subDirs;
    for (const auto& e : entries) {
        if (e.isDirectory) subDirs.push_back(e.path);
    }

    if (!subDirs.empty()) {
        SizeCalculator* subCalc = new SizeCalculator(this);
        connect(subCalc, &SizeCalculator::sizeReady,
                this,    &MainWindow::onSizeReady);

        connect(subCalc, &SizeCalculator::finished,
                this, [this, item](){
            // 如果頂層還沒算好 -> 取代
            // 如果頂層已經算好 -> 不覆蓋
            if(item->text(1) != "計算中...") return;

            qint64 total = 0;
            for(int i=0;i<item->childCount();++i){
                QTreeWidgetItem* child = item->child(i);
                if(child->text(1) == "計算中...") return;

                total += child->data(1, Qt::UserRole).value<qint64>();
            }

            item->setText(1, QString::fromStdString(FormatUtils::formatSize(total)));
            item->setData(1, Qt::UserRole, QVariant::fromValue(total));
        });

        subCalc->addPaths(subDirs);
        subCalc->start();
    }
}

QTreeWidgetItem* MainWindow::findItemByPath(const QString& path)
{
    // QTreeWidgetItemIterator：Qt 提供的疊代器，可以遍歷整棵樹
    // 不需要自己寫遞迴，Qt 幫你處理所有層級
    QTreeWidgetItemIterator it(contentTree_);
    while (*it) {
        if ((*it)->text(3) == path)
            return *it;
        ++it;
    }
    return nullptr;  // 找不到回傳 nullptr
}

void MainWindow::onSizeReady(const QString& path, qint64 size)
{
    // 找到對應的樹狀列並更新大小
    QTreeWidgetItem* item = findItemByPath(path);
    if (item) {
        if(item->text(1) == "計算中..."){
            item->setText(1, QString::fromStdString(FormatUtils::formatSize(size)));
            item->setData(1, Qt::UserRole, QVariant::fromValue(size));
        }
    }

    // 檢查頂層項目是否全部計算完成
    bool allDone = true;
    for (int i = 0; i < contentTree_->topLevelItemCount(); ++i) {
        if (contentTree_->topLevelItem(i)->text(1) == "計算中...") {
            allDone = false;
            break;
        }
    }
    if (allDone) {
        statusLabel_->setText("掃描完成");
        scanButton_->setEnabled(true);
    }
}

void MainWindow::onCategoryChanged(int row)
{
    // 對應左側類別清單的順序
    const QStringList categories = {
        "", // 全部
        "遊戲", "工作", "影片", "圖片",
        "文件", "音樂", "壓縮檔", "應用程式",
        "程式碼", "下載", "未知"
    };

    QString selected = categories[row];

    for (int i = 0; i < contentTree_->topLevelItemCount(); ++i) {
        QTreeWidgetItem* item = contentTree_->topLevelItem(i);

        if (selected.isEmpty()) {
            // 全部：顯示所有項目
            item->setHidden(false);
        } else {
            // setHidden(true)：隱藏不符合類別的項目
            // text(2) : 類別
            item->setHidden(item->text(2) != selected);
        }
    }
}

// moc 檔案必須在 .cpp 最後引入
#include "moc_MainWindow.cpp"