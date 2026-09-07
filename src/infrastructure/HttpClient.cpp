#include "infrastructure/HttpClient.h"

#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QTimer>
#include <QUuid>

namespace zhu_screen_pet {

HttpClient::HttpClient(QObject* parent)
    : QObject(parent), manager_(new QNetworkAccessManager(this))
{
}

QString HttpClient::get(const QUrl& url,
                        const QList<QPair<QByteArray, QByteArray>>& headers,
                        int timeoutMs)
{
    QNetworkRequest request(url);
    configureRequest(request, headers);
    return send(manager_->get(request), timeoutMs);
}

QString HttpClient::postJson(
    const QUrl& url, const QByteArray& body,
    const QList<QPair<QByteArray, QByteArray>>& headers,
    int timeoutMs, bool bufferResponse)
{
    QNetworkRequest request(url);
    configureRequest(request, headers);
    request.setHeader(QNetworkRequest::ContentTypeHeader,
                      QStringLiteral("application/json"));
    return send(manager_->post(request, body), timeoutMs, bufferResponse);
}

void HttpClient::cancel(const QString& requestId)
{
    QNetworkReply* reply = replies_.value(requestId, nullptr);
    if (reply != nullptr) {
        reply->abort();
    }
}

QString HttpClient::send(QNetworkReply* reply, int timeoutMs, bool bufferResponse)
{
    const QString requestId = QUuid::createUuid().toString(QUuid::Id128);
    replies_.insert(requestId, reply);
    responseBuffers_.insert(requestId, QByteArray{});
    responseSizes_.insert(requestId, 0);
    bufferResponses_.insert(requestId, bufferResponse);
    oversizedResponses_.insert(requestId, false);

    auto* timer = new QTimer(this);
    timer->setSingleShot(true);
    timers_.insert(requestId, timer);
    connect(timer, &QTimer::timeout, this, [this, requestId]() {
        finish(requestId, true);
    });
    connect(reply, &QNetworkReply::readyRead, this, [this, requestId, reply]() {
        const QByteArray data = reply->readAll();
        const qint64 total = responseSizes_.value(requestId) + data.size();
        responseSizes_[requestId] = total;
        if (total > MaximumResponseBytes) {
            oversizedResponses_[requestId] = true;
            reply->abort();
            return;
        }
        const QString contentType = reply->header(
            QNetworkRequest::ContentTypeHeader).toString().toLower();
        const bool isEventStream = contentType.startsWith(QStringLiteral("text/event-stream"));
        // 流式请求若实际返回普通 JSON，仍保留响应体供 Provider 回退解析。
        if (bufferResponses_.value(requestId) || !isEventStream) {
            responseBuffers_[requestId].append(data);
        }
        emit dataAvailable(requestId, data);
    });
    connect(reply, &QNetworkReply::finished, this, [this, requestId]() {
        finish(requestId, false);
    });
    timer->start(qMax(1, timeoutMs));
    return requestId;
}

void HttpClient::finish(const QString& requestId, bool timedOut)
{
    QNetworkReply* reply = replies_.take(requestId);
    QTimer* timer = timers_.take(requestId);
    if (reply == nullptr) {
        return;
    }
    if (timer != nullptr) {
        timer->stop();
        timer->deleteLater();
    }
    if (timedOut) {
        reply->abort();
    }

    HttpResponse response;
    response.statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    response.contentType = reply->header(QNetworkRequest::ContentTypeHeader).toString();
    response.body = responseBuffers_.take(requestId);
    response.responseTooLarge = oversizedResponses_.take(requestId);
    const bool bufferResponse = bufferResponses_.take(requestId);
    const bool isEventStream = response.contentType.toLower().startsWith(
        QStringLiteral("text/event-stream"));
    qint64 responseSize = responseSizes_.take(requestId);
    const QByteArray trailingData = reply->readAll();
    if (!trailingData.isEmpty() && !response.responseTooLarge
        && responseSize + trailingData.size() <= MaximumResponseBytes) {
        responseSize += trailingData.size();
        if (bufferResponse || !isEventStream) response.body.append(trailingData);
        emit dataAvailable(requestId, trailingData);
    } else if (!trailingData.isEmpty()) {
        response.responseTooLarge = true;
    }
    response.networkError = static_cast<int>(reply->error());
    response.errorString = response.responseTooLarge
        ? QStringLiteral("HTTP response exceeds the 8 MiB limit")
        : reply->errorString();
    response.timedOut = timedOut;
    emit requestFinished(requestId, response);
    reply->deleteLater();
}

void HttpClient::configureRequest(
    QNetworkRequest& request,
    const QList<QPair<QByteArray, QByteArray>>& headers) const
{
    for (const auto& header : headers) {
        request.setRawHeader(header.first, header.second);
    }
}

} // namespace zhu_screen_pet
