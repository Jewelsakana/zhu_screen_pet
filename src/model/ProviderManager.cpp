#include "model/ProviderManager.h"

#include <QTimer>
#include <QUuid>

#include "model/ChatProviderFactory.h"

namespace zhu_screen_pet {

ProviderManager::ProviderManager(ChatProviderFactory* factory, QObject* parent)
    : ChatProvider(parent), factory_(factory)
{
    qRegisterMetaType<ChatResult>("ChatResult");
}

bool ProviderManager::switchProvider(const ModelProviderConfig& source, QString* errorMessage)
{
    if (!activeRequests_.isEmpty()) {
        if (errorMessage) *errorMessage = QStringLiteral("cannot switch model while a request is running");
        return false;
    }
    if (factory_ == nullptr) {
        if (errorMessage) *errorMessage = QStringLiteral("chat provider factory is unavailable");
        return false;
    }
    const ModelProviderConfig config = source.normalized();
    std::unique_ptr<ChatProvider> candidate = factory_->create(config, errorMessage);
    if (candidate == nullptr) return false;

    if (activeProvider_ != nullptr) disconnect(activeProvider_.get(), nullptr, this, nullptr);
    activeProvider_ = std::move(candidate);
    activeConfig_ = config;
    connectProvider(activeProvider_.get());
    emit activeProviderChanged(activeConfig_.profileId, activeProviderName());
    return true;
}

QString ProviderManager::startChat(const std::vector<Message>& messages,
                                   const ChatOptions& options)
{
    if (activeProvider_ == nullptr) {
        const QString requestId = QUuid::createUuid().toString(QUuid::Id128);
        emit chatStarted(requestId);
        QTimer::singleShot(0, this, [this, requestId]() {
            emit chatFinished(requestId, ChatResult::failure(
                {ModelErrorCode::InvalidRequest, QStringLiteral("no model provider is active"), 0}));
        });
        return requestId;
    }
    const QString requestId = activeProvider_->startChat(messages, options);
    if (!requestId.isEmpty()) activeRequests_.insert(requestId);
    return requestId;
}

void ProviderManager::cancel(const QString& requestId)
{
    if (activeProvider_ != nullptr && activeRequests_.contains(requestId)) {
        activeProvider_->cancel(requestId);
    }
}

ModelProviderConfig ProviderManager::activeConfiguration() const
{
    return activeConfig_;
}

QString ProviderManager::activeProviderName() const
{
    return activeProvider_ == nullptr ? QString{} : activeConfig_.displayName;
}

int ProviderManager::activeRequestCount() const
{
    return activeRequests_.size();
}

bool ProviderManager::hasProvider() const
{
    return activeProvider_ != nullptr;
}

void ProviderManager::connectProvider(ChatProvider* provider)
{
    connect(provider, &ChatProvider::chatStarted, this,
            [this](const QString& requestId) { emit chatStarted(requestId); },
            Qt::QueuedConnection);
    connect(provider, &ChatProvider::chatDelta, this,
            [this](const QString& requestId, const QString& delta) {
                emit chatDelta(requestId, delta);
            }, Qt::QueuedConnection);
    connect(provider, &ChatProvider::chatFinished, this,
            [this](const QString& requestId, const ChatResult& result) {
                activeRequests_.remove(requestId);
                emit chatFinished(requestId, result);
            }, Qt::QueuedConnection);
}

} // namespace zhu_screen_pet
