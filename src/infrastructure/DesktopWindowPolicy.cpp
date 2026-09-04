#include "infrastructure/DesktopWindowPolicy.h"

#include <QGuiApplication>
#include <QWidget>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace zhu_screen_pet {

bool DesktopWindowPolicy::apply(QWidget* window, const DesktopWindowOptions& options,
                                QString* errorMessage)
{
    if (window == nullptr) {
        if (errorMessage) *errorMessage = QStringLiteral("window is null");
        return false;
    }
    window->setWindowFlag(Qt::FramelessWindowHint, options.frameless);
    window->setWindowFlag(Qt::WindowStaysOnTopHint, options.alwaysOnTop);
    window->setWindowFlag(Qt::Tool, !options.showInTaskbar);
    window->setWindowFlag(Qt::WindowDoesNotAcceptFocus, !options.acceptFocus);
    window->setAttribute(Qt::WA_TranslucentBackground, options.translucentBackground);
    window->setAttribute(Qt::WA_ShowWithoutActivating, !options.acceptFocus);
    window->setAttribute(Qt::WA_QuitOnClose, false);
    return setMouseInputTransparent(window, options.mouseInputTransparent, errorMessage);
}

bool DesktopWindowPolicy::setMouseInputTransparent(QWidget* window, bool enabled,
                                                   QString* errorMessage)
{
    if (window == nullptr) {
        if (errorMessage) *errorMessage = QStringLiteral("window is null");
        return false;
    }
    window->setAttribute(Qt::WA_TransparentForMouseEvents, enabled);
#ifdef Q_OS_WIN
    if (!window->isWindow() || QGuiApplication::platformName() != QStringLiteral("windows")) {
        return true;
    }
    HWND handle = reinterpret_cast<HWND>(window->winId());
    SetLastError(ERROR_SUCCESS);
    LONG_PTR style = GetWindowLongPtrW(handle, GWL_EXSTYLE);
    if (style == 0 && GetLastError() != ERROR_SUCCESS) {
        if (errorMessage) *errorMessage = QStringLiteral("GetWindowLongPtr failed: %1").arg(GetLastError());
        return false;
    }
    if (enabled) style |= WS_EX_TRANSPARENT | WS_EX_LAYERED;
    else style &= ~static_cast<LONG_PTR>(WS_EX_TRANSPARENT);
    SetLastError(ERROR_SUCCESS);
    const LONG_PTR previous = SetWindowLongPtrW(handle, GWL_EXSTYLE, style);
    if (previous == 0 && GetLastError() != ERROR_SUCCESS) {
        if (errorMessage) *errorMessage = QStringLiteral("SetWindowLongPtr failed: %1").arg(GetLastError());
        return false;
    }
    SetWindowPos(handle, nullptr, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
#else
    Q_UNUSED(errorMessage);
#endif
    return true;
}

} // namespace zhu_screen_pet
