#include "infrastructure/WindowManager.h"

#include "infrastructure/SettingsRepository.h"
#include "infrastructure/WindowPlacement.h"

#include <QApplication>
#include <QScreen>
#include <QWidget>

namespace zhu_screen_pet {

WindowManager::WindowManager(SettingsRepository* settings)
    : settings_(settings)
{
}

QPoint WindowManager::clampPosition(const QRect& availableGeometry,
                                    const QSize& windowSize,
                                    const QPoint& desiredPosition)
{
    return WindowPlacement::clamp(availableGeometry, windowSize, desiredPosition);
}

void WindowManager::restore(QWidget* window) const
{
    restore(window, QStringLiteral("main"));
}

void WindowManager::restore(QWidget* window, const QString& windowId,
                            const QPoint& fallback) const
{
    if (window == nullptr) {
        return;
    }
    const QString id = windowId.trimmed().isEmpty() ? QStringLiteral("main") : windowId.trimmed();
    const QString prefix = id == QStringLiteral("main")
        ? QStringLiteral("window/") : QStringLiteral("windows/%1/").arg(id);
    const QPoint position = settings_ == nullptr
        ? fallback
        : QPoint(settings_->value(prefix + QStringLiteral("x"), fallback.x()).toInt(),
                 settings_->value(prefix + QStringLiteral("y"), fallback.y()).toInt());
    window->move(position);
    clamp(window);
}

bool WindowManager::save(const QWidget* window, QString* errorMessage)
{
    return save(window, QStringLiteral("main"), errorMessage);
}

bool WindowManager::save(const QWidget* window, const QString& windowId,
                         QString* errorMessage)
{
    if (settings_ == nullptr || window == nullptr) {
        if (errorMessage) *errorMessage = QStringLiteral("window or settings is unavailable");
        return false;
    }
    const QString id = windowId.trimmed().isEmpty() ? QStringLiteral("main") : windowId.trimmed();
    const QString prefix = id == QStringLiteral("main")
        ? QStringLiteral("window/") : QStringLiteral("windows/%1/").arg(id);
    settings_->setValue(prefix + QStringLiteral("x"), window->pos().x());
    settings_->setValue(prefix + QStringLiteral("y"), window->pos().y());
    return settings_->save(errorMessage);
}

void WindowManager::clamp(QWidget* window) const
{
    if (window == nullptr) {
        return;
    }
    QScreen* screen = QGuiApplication::screenAt(window->frameGeometry().center());
    if (screen == nullptr) {
        screen = QGuiApplication::primaryScreen();
    }
    if (screen != nullptr) {
        window->move(clampPosition(screen->availableGeometry(), window->frameSize(), window->pos()));
    }
}

} // namespace zhu_screen_pet
