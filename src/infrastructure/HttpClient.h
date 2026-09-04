#pragma once

#include <QByteArray>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QObject>
#include <QUrl>
#include <QHash>

class QTimer;

namespace zhu_screen_pet {

/** 单次 HTTP 请求的传输结果；不包含任何模型协议语义。 */
struct HttpResponse
{
    int statusCode = 0;
    QByteArray body;
    int networkError = 0;
    QString errorString;
    bool timedOut = false;

    /** 返回网络传输成功且 HTTP 状态为 2xx。 */
    bool succeeded() const { return !timedOut && networkError == 0
        && statusCode >= 200 && statusCode < 300; }
};

Q_DECLARE_METATYPE(zhu_screen_pet::HttpResponse)

/** 基于 QNetworkAccessManager 的非阻塞 HTTP 传输服务。 */
class HttpClient final : public QObject
{
    Q_OBJECT

public:
    /** 创建 HTTP 客户端；网络请求在 Qt 事件循环中异步执行。 */
    explicit HttpClient(QObject* parent = nullptr);

    /** 发起 GET 请求；返回的 reply 由 Qt 管理，完成后通过信号通知。 */
    QString get(const QUrl& url,
                const QList<QPair<QByteArray, QByteArray>>& headers = {},
                int timeoutMs = 30000);
    /** 发起 JSON POST 请求并自动设置 Content-Type。 */
    QString postJson(const QUrl& url, const QByteArray& body,
                     const QList<QPair<QByteArray, QByteArray>>& headers = {},
                     int timeoutMs = 30000);
    /** 中止一个仍在进行的网络请求。 */
    void cancel(const QString& requestId);

signals:
    /** 请求完成；HTTP 状态码和响应体由 response 提供。 */
    void requestFinished(const QString& requestId, const HttpResponse& response);
    /** 数据到达时发出，供后续 SSE 流式 Provider 使用。 */
    void dataAvailable(const QString& requestId, const QByteArray& data);

private:
    /** 将调用方提供的原始请求头复制到 Qt 请求对象。 */
    void configureRequest(QNetworkRequest& request,
                          const QList<QPair<QByteArray, QByteArray>>& headers) const;
    /** 发起请求并注册超时与完成处理。 */
    QString send(QNetworkReply* reply, int timeoutMs);
    /** 完成并清理请求上下文。 */
    void finish(const QString& requestId, bool timedOut = false);

    QNetworkAccessManager* manager_ = nullptr;
    QHash<QString, QNetworkReply*> replies_;
    QHash<QString, QTimer*> timers_;
    QHash<QString, QByteArray> responseBuffers_;
};

} // namespace zhu_screen_pet
