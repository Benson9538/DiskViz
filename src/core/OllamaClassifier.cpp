#include "core/OllamaClassifier.h"
#include <QJsonObject>
#include <QJsonDocument>
#include <QNetworkRequest>
#include <QUrl>
#include <algorithm>
#include <iostream>

// qgetenv：讀取環境變數，回傳 QByteArray
// 若環境變數未設定，使用預設值（WSL → Windows 的固定 IP）
static QString ollamaUrl() {
    QByteArray host = qgetenv("OLLAMA_HOST");
    if (!host.isEmpty())
        return QString::fromUtf8(host) + "/api/generate";
    return "http://172.25.112.1:11434/api/generate";
}

static const QString OLLAMA_MODEL = "llama3.2";

OllamaClassifier::OllamaClassifier(QObject* parent)
    : QObject(parent)
    , manager_(new QNetworkAccessManager(this))
    // manager_ 以 this 為 parent，MainWindow 關閉時會自動釋放
{}

void OllamaClassifier::classify(const std::vector<ClassifyRequest>& requests)
{
    if (requests.empty()) {
        emit finished();
        return;
    }

    pending_           = requests;
    completedBatches_  = 0;

    // 無條件進位：200 個項目 / 每批 20 = 10 批
    // (n + batchSize - 1) / batchSize 是整數無條件進位的標準寫法
    totalBatches_ = ((int)pending_.size() + batchSize_ - 1) / batchSize_;

    std::cout << "[Ollama] 開始分類，共 " << pending_.size()
              << " 個項目，" << totalBatches_ << " 批\n";

    sendBatch(0);  // 從第 0 批開始，結果回來後再送下一批（sequential）
}

void OllamaClassifier::sendBatch(int batchIndex)
{
    // 計算這批的範圍：[start, end)
    int start = batchIndex * batchSize_;
    int end   = std::min(start + batchSize_, (int)pending_.size());

    // 從 pending_ 切出這一批
    std::vector<ClassifyRequest> batch(
        pending_.begin() + start,
        pending_.begin() + end);

    // ── 組建 Prompt ──────────────────────────────────────
    // 用英文下指令（模型遵循率更高），但限制只能輸出我們定義的中文分類
    // 給 few-shot 範例讓模型學習輸出格式
    QString prompt =
        "Classify each file by its filename. "
        "Choose ONLY from: 遊戲, 工作, 影片, 圖片, 文件, 音樂, 壓縮檔, 應用程式, 程式碼, 下載, 未知\n"
        "Output ONLY lines in format: NUMBER:CATEGORY (no spaces, no explanation)\n"
        "Example:\n"
        "1:影片\n"
        "2:遊戲\n"
        "Files:\n";

    for (int i = 0; i < (int)batch.size(); ++i) {
        prompt += QString("%1. %2\n")
                    .arg(i + 1)
                    .arg(QString::fromStdString(batch[i].filename));
    }

    // ── 組建 HTTP 請求 ────────────────────────────────────
    QJsonObject body;
    body["model"]  = OLLAMA_MODEL;
    body["prompt"] = prompt;
    body["stream"] = false;  // 等全部生成完才回傳，不要逐字 stream

    QUrl url(ollamaUrl());  // 先建 QUrl，避免 most vexing parse
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    // manager_->post() 是非同步的：送出後立刻返回，不等結果
    // reply 是這次請求的物件，結果回來時會發射 reply->finished() signal
    QNetworkReply* reply = manager_->post(
        request, QJsonDocument(body).toJson());

    std::cout << "[Ollama] 送出第 " << batchIndex + 1
              << "/" << totalBatches_ << " 批（"
              << batch.size() << " 個）\n";

    // ── 處理回應 ──────────────────────────────────────────
    // Lambda 捕獲 batch 和 batchIndex，因為 sendBatch() 返回後這些變數會消失
    // 但 lambda 在 reply 回來時才執行，必須把需要的資料捕獲進去
    connect(reply, &QNetworkReply::finished,
            this,  [this, reply, batch, batchIndex]() {

        // 確保 reply 在這個 lambda 結束後被釋放
        // deleteLater 安全，不是立刻刪
        reply->deleteLater();

        if (reply->error() != QNetworkReply::NoError) {
            std::cerr << "[Ollama] 第 " << batchIndex + 1
                      << " 批失敗：" << reply->errorString().toStdString() << "\n";
            // 失敗就跳過這批，繼續下一批（不讓一次失敗中斷全部）
        } else {
            QByteArray data = reply->readAll();
            parseBatchResponse(data, batch);
        }

        completedBatches_++;
        emit progressUpdated(completedBatches_, totalBatches_);

        // 還有下一批就繼續送，否則全部完成
        if (completedBatches_ < totalBatches_) {
            sendBatch(completedBatches_);
        } else {
            std::cout << "[Ollama] 全部分類完成\n";
            emit finished();
        }
    });
}

void OllamaClassifier::parseBatchResponse(const QByteArray& data,
                                           const std::vector<ClassifyRequest>& batch)
{
    // Ollama 回傳的外層是 JSON：{ "response": "1:影片\n2:遊戲\n", "done": true }
    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (!doc.isObject()) {
        std::cerr << "[Ollama] 回應不是 JSON\n";
        return;
    }

    QString response = doc.object()["response"].toString().trimmed();
    std::cout << "[Ollama] 回應：\n" << response.toStdString() << "\n";

    // 逐行解析，每行格式是 "1:影片"
    QStringList lines = response.split('\n', Qt::SkipEmptyParts);

    for (const QString& line : lines) {
        int colonPos = line.indexOf(':');
        if (colonPos < 0) continue;  // 沒有冒號，不是我們要的格式

        bool ok;
        int index = line.left(colonPos).trimmed().toInt(&ok);

        // index 從 1 開始，要在有效範圍內
        if (!ok || index < 1 || index > (int)batch.size()) continue;

        QString raw      = line.mid(colonPos + 1).trimmed();
        QString category = normalizeCategory(raw);

        // 用 path 當 key，讓 MainWindow 找到對應的 TreeWidgetItem
        emit resultReady(
            QString::fromStdString(batch[index - 1].path),
            category);
    }
}

QString OllamaClassifier::normalizeCategory(const QString& raw)
{
    // Ollama 有時用英文、有時用中文，大小寫也不固定
    // 全部 toLower 後做關鍵字比對，轉成系統統一的中文分類名稱
    // 順序很重要：越具體的放越前面，避免被模糊匹配搶走
    QString s = raw.toLower().trimmed();

    // 程式碼要在應用程式之前，避免 "code" 被 "app" 以外的條件截走
    if (s.contains("程式碼") || s.contains("code") ||
        s.contains("source") || s.contains("programming"))     return "程式碼";

    if (s.contains("遊戲") || s.contains("game"))              return "遊戲";
    if (s.contains("工作") || s.contains("work") ||
        s.contains("office") || s.contains("business"))        return "工作";
    if (s.contains("影片") || s.contains("video") ||
        s.contains("movie") || s.contains("film"))             return "影片";
    if (s.contains("圖片") || s.contains("image") ||
        s.contains("photo") || s.contains("picture"))          return "圖片";
    if (s.contains("文件") || s.contains("document") ||
        s.contains("doc") || s.contains("file") ||
        s.contains("spreadsheet"))                             return "文件";
    if (s.contains("音樂") || s.contains("music") ||
        s.contains("audio") || s.contains("sound"))            return "音樂";
    if (s.contains("壓縮") || s.contains("archive") ||
        s.contains("zip") || s.contains("compress"))           return "壓縮檔";
    if (s.contains("應用") || s.contains("application") ||
        s.contains("software") || s.contains("app") ||
        s.contains("程式"))                                    return "應用程式";
    if (s.contains("下載") || s.contains("download"))          return "下載";

    // 明確的 unknown 也對應回去
    if (s.contains("unknown") || s.contains("未知") ||
        s.contains("other") || s.contains("misc"))             return "未知";

    return "未知";
}

#include "moc_OllamaClassifier.cpp"
