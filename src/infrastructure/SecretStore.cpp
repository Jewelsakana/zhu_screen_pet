#include "infrastructure/SecretStore.h"

#ifdef Q_OS_WIN
#include <windows.h>
#include <wincred.h>
#endif

namespace zhu_screen_pet {

#ifdef Q_OS_WIN

namespace {

/** 生成 Credential Manager 使用的稳定凭据名称。 */
QString targetName(const QString& service, const QString& account)
{
    return service + QStringLiteral(":") + account;
}

} // namespace

bool SecretStore::write(const QString& service, const QString& account,
                        const QString& secret, QString* errorMessage) const
{
    const QString target = targetName(service, account);
    const QByteArray targetUtf8 = target.toUtf8();
    const QByteArray secretUtf8 = secret.toUtf8();

    CREDENTIALW credential{};
    credential.Type = CRED_TYPE_GENERIC;
    credential.TargetName = const_cast<wchar_t*>(reinterpret_cast<const wchar_t*>(target.utf16()));
    credential.CredentialBlobSize = static_cast<DWORD>(secretUtf8.size());
    credential.CredentialBlob = reinterpret_cast<LPBYTE>(const_cast<char*>(secretUtf8.constData()));
    credential.Persist = CRED_PERSIST_LOCAL_MACHINE;
    credential.UserName = const_cast<wchar_t*>(reinterpret_cast<const wchar_t*>(account.utf16()));

    if (CredWriteW(&credential, 0) != TRUE) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("CredWrite failed: %1").arg(GetLastError());
        }
        return false;
    }
    Q_UNUSED(targetUtf8);
    return true;
}

bool SecretStore::read(const QString& service, const QString& account,
                       QString* secret, QString* errorMessage) const
{
    if (secret == nullptr) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("secret output is null");
        }
        return false;
    }

    PCREDENTIALW credential = nullptr;
    const QString target = targetName(service, account);
    if (CredReadW(reinterpret_cast<LPCWSTR>(target.utf16()), CRED_TYPE_GENERIC, 0,
                  &credential) != TRUE) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("CredRead failed: %1").arg(GetLastError());
        }
        return false;
    }

    *secret = QString::fromUtf8(reinterpret_cast<const char*>(credential->CredentialBlob),
                                static_cast<int>(credential->CredentialBlobSize));
    CredFree(credential);
    return true;
}

bool SecretStore::remove(const QString& service, const QString& account,
                         QString* errorMessage) const
{
    const QString target = targetName(service, account);
    if (CredDeleteW(reinterpret_cast<LPCWSTR>(target.utf16()), CRED_TYPE_GENERIC, 0) != TRUE) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("CredDelete failed: %1").arg(GetLastError());
        }
        return false;
    }
    return true;
}

#else

bool SecretStore::write(const QString&, const QString&, const QString&, QString* errorMessage) const
{
    if (errorMessage != nullptr) *errorMessage = QStringLiteral("SecretStore is only implemented on Windows");
    return false;
}

bool SecretStore::read(const QString&, const QString&, QString*, QString* errorMessage) const
{
    if (errorMessage != nullptr) *errorMessage = QStringLiteral("SecretStore is only implemented on Windows");
    return false;
}

bool SecretStore::remove(const QString&, const QString&, QString* errorMessage) const
{
    if (errorMessage != nullptr) *errorMessage = QStringLiteral("SecretStore is only implemented on Windows");
    return false;
}

#endif

} // namespace zhu_screen_pet
