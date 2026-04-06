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
    , ollamaClassifier_(new OllamaClassifier(this))
{
    setWindowTitle("DiskViz");
    resize(1280, 800);
    setupUI();

    connect(scanWorker_, &ScanWorker::scanResultReady,
            this,        &MainWindow::onScanResultReady);
    connect(scanWorker_, &ScanWorker::scanFinished,
            this,        &MainWindow::onScanFinished);

    connect(ollamaClassifier_, &OllamaClassifier::resultReady,
            this,              &MainWindow::onOllamaResult);
    connect(ollamaClassifier_, &OllamaClassifier::finished,
            this,              &MainWindow::onOllamaFinished);

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
        updateChart(cachedResults);
        onCategoryChanged(categoryList_->currentRow());

        QString timeStr = oldestScanTime.toString("yyyy/MM/dd hh:mm:ss");
        statusLabel_->setText("上次更新 : " + timeStr + " ，更新中...");
    }
    else statusLabel_->setText("首次掃描中");

    scanButton_->setEnabled(false);
    scanWorker_->scan(scanPaths);
}

QTreeWidgetItem* MainWindow::findOrCreateGroupHeader(const fs::path& rootPath)
{
    QString name = QString::fromStdString(rootPath.filename().string());

    // 搜尋是否已有這個 rootPath 的標題列
    for (int i = 0; i < contentTree_->topLevelItemCount(); ++i) {
        QTreeWidgetItem* item = contentTree_->topLevelItem(i);
        if (item->text(0) == name)
            return item;
    }

    // 沒有就建立新的
    QTreeWidgetItem* header = new QTreeWidgetItem(contentTree_);
    header->setText(0, name);
    header->setFirstColumnSpanned(true);
    header->setBackground(0, QColor(230, 230, 230));
    header->setFlags(header->flags() & ~Qt::ItemIsSelectable);
    QFont font; font.setBold(true);
    header->setFont(0, font);
    header->setExpanded(true);
    return header;
}

void MainWindow::appendResultToTree(const ScanResult& r)
{
    QTreeWidgetItem* header = findOrCreateGroupHeader(r.rootPath);

    // 移除 loading 佔位符（第一個子項目若是「掃描中...」就刪掉）
    if (header->childCount() == 1 &&
        header->child(0)->text(0) == "掃描中...")
        delete header->takeChild(0);

    QTreeWidgetItem* item = new SortableTreeItem(header);
    item->setText(0, QString::fromStdString(r.path.filename().string()));

    if (r.isDirectory) {
        item->setText(1, "計算中...");
        item->setData(1, Qt::UserRole, QVariant::fromValue((qint64)0));
        item->setChildIndicatorPolicy(QTreeWidgetItem::ShowIndicator);
    } else {
        item->setText(1, QString::fromStdString(FormatUtils::formatSize(r.totalSize)));
        item->setData(1, Qt::UserRole, QVariant::fromValue(r.totalSize));
    }

    item->setText(2, QString::fromStdString(r.category));
    item->setText(3, QString::fromStdString(r.path.string()));
}

void MainWindow::onScanResultReady(const ScanResult& r)
{
    appendResultToTree(r);
    // onCategoryChanged 不在這裡呼叫
    // 每筆都呼叫會讓主執行緒在 D 槽這種大目錄下被塞爆
    // 改為只在 onScanFinished 最後套用一次
}

void MainWindow::onScanFinished(const vector<ScanResult>& results)
{
    // Tree 已由 onScanResultReady 增量填好，這裡只更新圖表和狀態
    updateChart(results);
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

    // 收集所有 "未知" 分類的項目，送給 Ollama 做 AI 分類
    std::vector<ClassifyRequest> unknowns;
    for (const auto& r : results) {
        if (r.category == "未知") {
            ClassifyRequest req;
            req.path     = r.path.string();
            req.filename = r.path.filename().string();
            unknowns.push_back(req);
        }
    }

    if (!unknowns.empty()) {
        statusLabel_->setText(
            "已更新 :" + timeStr +
            QString("　AI 分類中（%1 個未知項目）...").arg(unknowns.size()));
        ollamaClassifier_->classify(unknowns);
    }

    // 收集所有目錄，背景計算大小（不阻塞 finished 的觸發）
    std::vector<fs::path> dirs;
    for (const auto& r : results)
        if (r.isDirectory) dirs.push_back(r.path);

    if (!dirs.empty()) {
        SizeCalculator* calc = new SizeCalculator(this);

        connect(calc, &SizeCalculator::sizeReady, this,
            [this](const QString& path, qint64 size) {
                // 更新 Tree
                QTreeWidgetItem* item = findItemByPath(path);
                if (item && item->text(1) == "計算中...") {
                    item->setText(1, QString::fromStdString(
                        FormatUtils::formatSize(size)));
                    item->setData(1, Qt::UserRole,
                        QVariant::fromValue(size));
                }
                // 更新快取，下次載入時顯示正確大小
                cacheManager_->updateSize(path, size);
            });

        connect(calc, &SizeCalculator::finished, this, [this, calc]() {
            calc->deleteLater();
            statusLabel_->setText(
                "大小計算完成 : " +
                QDateTime::currentDateTime().toString("yyyy/MM/dd hh:mm:ss"));
            scanButton_->setEnabled(true);
        });

        calc->addPaths(dirs);
        calc->start();
    }
}

void MainWindow::setupUI()
{
    // QSplitter：把左右兩個元件包起來
    // 使用者可以拖動中間的分隔線調整比例
    splitter_ = new QSplitter(Qt::Horizontal, this);
    setCentralWidget(splitter_);

    setupSidebar();

    pageStack_ = new QStackedWidget(splitter_);

    setupOverviewPage(); // idx 0
    setupContentPage();  // idx 1

    pageStack_->setCurrentIndex(1); // 預設顯示頁

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

    // 頁面切換按鈕
    QHBoxLayout* btnLayout = new QHBoxLayout();
    btnOverview_ = new QPushButton("總覽");
    btnContent_ = new QPushButton("內容");

    btnOverview_->setCheckable(true); // 可呈現按下的狀態
    btnContent_->setCheckable(true);
    btnContent_->setChecked(true);   // 預設選中頁

    btnLayout->addWidget(btnOverview_);
    btnLayout->addWidget(btnContent_);
    layout->addLayout(btnLayout);

    connect(btnOverview_, &QPushButton::clicked,
            this,         &MainWindow::onOverviewClicked);
    connect(btnContent_,  &QPushButton::clicked,
            this,         &MainWindow::onContentClicked);

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

void MainWindow::setupOverviewPage()
{
    QWidget* overviewPage = new QWidget();
    QVBoxLayout* layout = new QVBoxLayout(overviewPage);
    layout->setContentsMargins(8,8,8,8);
    
    QLabel* title = new QLabel("使用空間總覽");
    title->setStyleSheet("font-size: 20px; font-weight: bold;");
    layout->addWidget(title);

    // 圓餅圖
    pieSeries_ = new QPieSeries();
    pieSeries_->append("尚無資料", 1);

    chart_ = new QChart();
    chart_->addSeries(pieSeries_);
    chart_->setTitle("掃描後自動更新");
    chart_->legend()->show();
    chart_->legend()->setAlignment(Qt::AlignRight);

    chartView_ = new QChartView(chart_);
    chartView_->setRenderHint(QPainter::Antialiasing);

    QLabel* warning = new QLabel(
        "以下數據根據常用路徑估算，並非磁碟完整使用量");
    warning->setStyleSheet(
        "color: #e67e00;"
        "font-size: 18px;"
        "font-weight: bold;"
        "padding: 8px 12px;"
        "background: #fff8e1;"
        "border-left: 4px solid #e67e00;"
        "border-radius: 4px;");
    warning->setWordWrap(true);

    layout->addWidget(warning);
    layout->addWidget(chartView_);

    pageStack_->addWidget(overviewPage);
}

void MainWindow::setupContentPage()
{
    QWidget* contentPage = new QWidget();
    QVBoxLayout* layout = new QVBoxLayout(contentPage);
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

    pageStack_->addWidget(contentPage);
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
    auto scanPaths = getSelectedScanPaths();

    if (scanPaths.empty()) {
        statusLabel_->setText("請至少選擇一個掃描位置");
        return;
    }

    // 清空舊內容，為每個掃描路徑建立 loading 標題列
    contentTree_->clear();
    for (const auto& path : scanPaths) {
        QTreeWidgetItem* header = new QTreeWidgetItem(contentTree_);
        header->setText(0, QString::fromStdString(path.filename().string()));
        header->setFirstColumnSpanned(true);
        header->setBackground(0, QColor(230, 230, 230));
        header->setFlags(header->flags() & ~Qt::ItemIsSelectable);
        QFont font; font.setBold(true);
        header->setFont(0, font);
        header->setExpanded(true);

        // 暫時的 loading 子項目
        QTreeWidgetItem* loading = new QTreeWidgetItem(header);
        loading->setText(0, "掃描中...");
        loading->setFirstColumnSpanned(true);
        loading->setForeground(0, QColor(150, 150, 150));
        loading->setFlags(loading->flags() & ~Qt::ItemIsSelectable);
    }

    statusLabel_->setText("掃描中...");
    scanButton_->setEnabled(false);
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
            QTreeWidgetItem* item = new SortableTreeItem(header);
            item->setText(0, QString::fromStdString(r.path.filename().string()));
            item->setText(1, QString::fromStdString(FormatUtils::formatSize(r.totalSize)));
            item->setData(1, Qt::UserRole, QVariant::fromValue(r.totalSize));
            // 直接用 r.category，保留快取中 Ollama 分類過的結果
            item->setText(2, QString::fromStdString(r.category));
            item->setText(3, QString::fromStdString(r.path.string()));

            if(r.isDirectory)
                item->setChildIndicatorPolicy(QTreeWidgetItem::ShowIndicator);
        }
    }   
}

void MainWindow::onOverviewClicked()
{
    pageStack_->setCurrentIndex(0);
    btnOverview_->setChecked(true);
    btnContent_->setChecked(false);
}

void MainWindow::onContentClicked()
{
    pageStack_->setCurrentIndex(1);
    btnContent_->setChecked(true);
    btnOverview_->setChecked(false);
}

void MainWindow::updateChart(const vector<ScanResult>& results)
{
    map<string, qint64> categoryTotals;
    for(const auto& r : results){
        categoryTotals[r.category] += r.totalSize;
    }

    pieSeries_->clear();

    for(const auto& [cat, size] : categoryTotals){
        if(size <= 0) continue;
        pieSeries_->append(QString::fromStdString(cat), size);
    }

    for(auto& slice : pieSeries_->slices()){        
        slice->setLabelVisible(true);
        slice->setLabelPosition(QPieSlice::LabelOutside);
        slice->setLabel(
            slice->label() + "\n" +
            QString::fromStdString(FormatUtils::formatSize(slice->value())) +
            QString("\n%1%").arg(slice->percentage() * 100, 0, 'f', 1)
        );
        QFont font;
        font.setPointSize(9);
        font.setBold(true);
        slice->setLabelFont(font);

        if(slice->percentage() < 0.03) slice->setLabelVisible(false);
    }
    QFont titleFont;
    titleFont.setPointSize(14);
    titleFont.setBold(true);
    chart_->setTitleFont(titleFont);
    chart_->setTitle("使用空間總覽");
    chart_->setBackgroundVisible(false);
    chart_->legend()->hide();
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

void MainWindow::onOllamaResult(const QString& path, const QString& category)
{
    // 在樹狀清單中找到這個路徑的項目，更新它的分類欄位
    QTreeWidgetItem* item = findItemByPath(path);
    if (item) {
        item->setText(2, category);
    }

    // 同步更新 SQLite 快取，下次開啟程式就直接顯示正確分類
    cacheManager_->updateCategory(path, category);
}

void MainWindow::onOllamaFinished()
{
    // Ollama 全部跑完，更新狀態列
    QString timeStr = QDateTime::currentDateTime()
                        .toString("yyyy/MM/dd hh:mm:ss");
    statusLabel_->setText("AI 分類完成 :" + timeStr);

    // 重新套用目前的類別篩選（有些項目的分類改變了）
    onCategoryChanged(categoryList_->currentRow());
}

void MainWindow::onCategoryChanged(int row)
{
    const QStringList categories = {
        "", // 全部
        "遊戲", "工作", "影片", "圖片",
        "文件", "音樂", "壓縮檔", "應用程式",
        "程式碼", "下載", "未知"
    };

    QString selected = categories[row];

    // 頂層是灰色分組標題列（Documents / Downloads...），不是實際檔案
    // 實際檔案是標題列的子節點，要對子節點做篩選
    for (int i = 0; i < contentTree_->topLevelItemCount(); ++i) {
        QTreeWidgetItem* header = contentTree_->topLevelItem(i);

        bool anyVisible = false;

        for (int j = 0; j < header->childCount(); ++j) {
            QTreeWidgetItem* child = header->child(j);

            bool show = selected.isEmpty() || child->text(2) == selected;
            child->setHidden(!show);
            if (show) anyVisible = true;
        }

        // 標題列底下全部被篩掉時，標題列本身也隱藏
        header->setHidden(!anyVisible);
    }
}

// moc 檔案必須在 .cpp 最後引入
#include "moc_MainWindow.cpp"