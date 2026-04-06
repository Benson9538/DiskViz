#include "core/OllamaClassifier.h"
#include <QJsonObject>
#include <QJsonDocument>
#include <QNetworkRequest>
#include <QCoreApplication>
#include <QUrl>
#include <algorithm>
#include <iostream>

// 從 .env 檔案讀取指定 key 的值
// 格式：KEY=value（每行一個，# 開頭為註解）
static QString readDotEnv(const QString& key)
{
    // 用執行檔所在目錄找 .env，不受工作目錄影響
    QString envPath = QCoreApplication::applicationDirPath() + "/.env";
    QFile file(envPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return {};

    QTextStream in(&file);
    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();
        if (line.startsWith('#') || !line.contains('=')) continue;

        int eq = line.indexOf('=');
        if (line.left(eq).trimmed() == key) {
            QString val = line.mid(eq + 1).trimmed();
            // 移除前後的引號（"value" 或 'value'）
            if ((val.startsWith('"') && val.endsWith('"')) ||
                (val.startsWith('\'') && val.endsWith('\'')))
                val = val.mid(1, val.length() - 2);
            return val;
        }
    }
    return {};
}

// 優先讀系統環境變數，其次讀 .env，都沒有就回傳空字串
static QString ollamaUrl()
{
    // qgetenv：讀取系統環境變數
    QByteArray envHost = qgetenv("OLLAMA_HOST");
    if (!envHost.isEmpty())
        return QString::fromUtf8(envHost) + "/api/generate";

    // 從 .env 讀取
    QString dotEnvHost = readDotEnv("OLLAMA_HOST");
    if (!dotEnvHost.isEmpty())
        return dotEnvHost + "/api/generate";

    return {};  // 都找不到，回傳空字串讓呼叫端報錯
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

    if (ollamaUrl().isEmpty()) {
        // OLLAMA_HOST 未設定，靜默略過，項目保留「未知」分類
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
    // 明確告訴 AI 輸出格式，減少不符合格式的回答
    QString prompt =
        "你是檔案分類系統，只能用繁體中文分類。\n"
        "將以下檔案分類為以下類別之一：\n"
        "遊戲, 工作, 影片, 圖片, 文件, 音樂, 壓縮檔, 應用程式, 程式碼, 下載, 未知\n\n"
        "輸出格式（每行一個，不要任何說明）：\n"
        "編號:類別\n\n"
        "範例：\n"
        "1:影片\n"
        "2:遊戲\n\n"
        "待分類檔案：\n";

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

    QUrl url(ollamaUrl());
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
    // Ollama 可能回傳各種變體（英文/中文/大小寫不同）
    // 全部轉成我們系統使用的中文分類名稱
    QString s = raw.toLower().trimmed();

    if (s.contains("遊戲") || s.contains("game"))              return "遊戲";
    if (s.contains("工作") || s.contains("work"))              return "工作";
    if (s.contains("影片") || s.contains("video"))             return "影片";
    if (s.contains("圖片") || s.contains("image") ||
        s.contains("photo"))                                    return "圖片";
    if (s.contains("文件") || s.contains("document") ||
        s.contains("doc"))                                      return "文件";
    if (s.contains("音樂") || s.contains("music") ||
        s.contains("audio"))                                    return "音樂";
    if (s.contains("壓縮") || s.contains("archive") ||
        s.contains("zip"))                                      return "壓縮檔";
    if (s.contains("應用") || s.contains("application") ||
        s.contains("app") || s.contains("程式"))               return "應用程式";
    if (s.contains("程式碼") || s.contains("code") ||
        s.contains("source"))                                   return "程式碼";
    if (s.contains("下載") || s.contains("download"))          return "下載";

    return "未知";
}

#include "moc_OllamaClassifier.cpp"
