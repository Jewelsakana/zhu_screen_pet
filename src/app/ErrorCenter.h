#pragma once

#include <QHash>
#include <QObject>

#include "core/AppError.h"

namespace zhu_screen_pet {

class Logger;

/** 应用错误汇聚点：统一脱敏记录错误，并将同一结构化错误通知展示层。 */
class ErrorCenter final : public QObject
{
    Q_OBJECT

public:
    explicit ErrorCenter(Logger* logger = nullptr, QObject* parent = nullptr);

    void setMessages(const QHash<QString, QString>& messages);
    QString userMessage(const AppError& error) const;

public slots:
    void report(const AppError& error);

signals:
    void errorReported(const AppError& error, const QString& userMessage);

private:
    Logger* logger_ = nullptr;
    QHash<QString, QString> messages_;
};

} // namespace zhu_screen_pet
