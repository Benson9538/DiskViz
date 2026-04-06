#pragma once

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QString>
#include <vector>

// 送給 Ollama 的單筆請求
struct ClassifyRequest {
    std::string path;      // 完整路徑（用來配對回傳結果）
    std::string filename;  // 只有檔名（給 AI 看，省 token）
};

// Ollama 回傳的單筆結果
struct ClassifyResult {
    std::string path;
    std::string category;
};

class OllamaClassifier : public QObject {
    Q_OBJECT

public:
    explicit OllamaClassifier(QObject* parent = nullptr);

    // 傳入所有待分類的項目，開始批次送 Ollama
    void classify(const std::vector<ClassifyRequest>& requests);

signals:
    // 每有一個結果回來就發射（不需要等全部完成才更新 UI）
    void resultReady(const QString& path, const QString& category);

    // 所有批次都處理完畢
    void finished();

    // 進度更新（已完成批次數, 總批次數）
    void progressUpdated(int done, int total);

private:
    QNetworkAccessManager* manager_;

    std::vector<ClassifyRequest> pending_;  // 全部待處理的請求
    int batchSize_         = 20;  // 每批送幾個給 Ollama
    int totalBatches_      = 0;
    int completedBatches_  = 0;

    // 送出第 batchIndex 批
    void sendBatch(int batchIndex);

    // 解析 Ollama 回傳的文字，發射 resultReady signal
    void parseBatchResponse(const QByteArray& data,
                            const std::vector<ClassifyRequest>& batch);

    // 把 Ollama 的自然語言回答正規化成固定的中文分類名稱
    QString normalizeCategory(const QString& raw);
};
