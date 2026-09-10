#include "infrastructure/InputActivityMonitor.h"

#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace zhu_screen_pet {

#ifdef Q_OS_WIN
namespace {
InputActivityMonitor* activeMonitor = nullptr;

LRESULT CALLBACK keyboardHookProcedure(int code, WPARAM message, LPARAM data)
{
    Q_UNUSED(data);
    if (code == HC_ACTION && activeMonitor != nullptr
        && (message == WM_KEYDOWN || message == WM_SYSKEYDOWN)) {
        emit activeMonitor->keyboardInputDetected();
    }
    return CallNextHookEx(nullptr, code, message, data);
}

LRESULT CALLBACK mouseHookProcedure(int code, WPARAM message, LPARAM data)
{
    Q_UNUSED(data);
    if (code == HC_ACTION && activeMonitor != nullptr) {
        switch (message) {
        case WM_LBUTTONDOWN:
        case WM_RBUTTONDOWN:
        case WM_MBUTTONDOWN:
        case WM_XBUTTONDOWN:
            emit activeMonitor->mouseInputDetected();
            break;
        default:
            break;
        }
    }
    return CallNextHookEx(nullptr, code, message, data);
}
}
#endif

InputActivityMonitor::InputActivityMonitor(QObject* parent)
    : QObject(parent)
{
}

InputActivityMonitor::~InputActivityMonitor()
{
    stop();
}

bool InputActivityMonitor::start(QString* errorMessage)
{
#ifdef Q_OS_WIN
    if (isRunning()) return true;
    if (activeMonitor != nullptr && activeMonitor != this) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("another global input monitor is already active");
        }
        return false;
    }
    activeMonitor = this;
    DWORD nativeError = ERROR_SUCCESS;
    keyboardHook_ = SetWindowsHookExW(WH_KEYBOARD_LL, keyboardHookProcedure,
                                      GetModuleHandleW(nullptr), 0);
    if (keyboardHook_ == nullptr) nativeError = GetLastError();
    mouseHook_ = SetWindowsHookExW(WH_MOUSE_LL, mouseHookProcedure,
                                   GetModuleHandleW(nullptr), 0);
    if (mouseHook_ == nullptr && nativeError == ERROR_SUCCESS) nativeError = GetLastError();
    if (keyboardHook_ != nullptr && mouseHook_ != nullptr) return true;

    stop();
    if (errorMessage != nullptr) {
        *errorMessage = QStringLiteral("failed to install global input hooks: %1")
            .arg(nativeError);
    }
    return false;
#else
    if (errorMessage != nullptr) {
        *errorMessage = QStringLiteral("global input counting is only supported on Windows");
    }
    return false;
#endif
}

void InputActivityMonitor::stop()
{
#ifdef Q_OS_WIN
    if (keyboardHook_ != nullptr) {
        UnhookWindowsHookEx(static_cast<HHOOK>(keyboardHook_));
        keyboardHook_ = nullptr;
    }
    if (mouseHook_ != nullptr) {
        UnhookWindowsHookEx(static_cast<HHOOK>(mouseHook_));
        mouseHook_ = nullptr;
    }
    if (activeMonitor == this) activeMonitor = nullptr;
#endif
}

bool InputActivityMonitor::isRunning() const
{
    return keyboardHook_ != nullptr && mouseHook_ != nullptr;
}

bool InputActivityMonitor::isSupported() const
{
#ifdef Q_OS_WIN
    return true;
#else
    return false;
#endif
}

} // namespace zhu_screen_pet
