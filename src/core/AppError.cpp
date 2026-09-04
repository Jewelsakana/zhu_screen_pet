#include "core/AppError.h"

namespace zhu_screen_pet {

QString AppError::codeName() const
{
    switch (code) {
    case AppErrorCode::None: return QStringLiteral("none");
    case AppErrorCode::InvalidArgument: return QStringLiteral("invalid_argument");
    case AppErrorCode::NotFound: return QStringLiteral("not_found");
    case AppErrorCode::Busy: return QStringLiteral("busy");
    case AppErrorCode::NotReady: return QStringLiteral("not_ready");
    case AppErrorCode::ConfigInvalid: return QStringLiteral("config_invalid");
    case AppErrorCode::Io: return QStringLiteral("io");
    case AppErrorCode::DatabaseUnavailable: return QStringLiteral("database_unavailable");
    case AppErrorCode::DatabaseQuery: return QStringLiteral("database_query");
    case AppErrorCode::Authentication: return QStringLiteral("authentication");
    case AppErrorCode::Timeout: return QStringLiteral("timeout");
    case AppErrorCode::Network: return QStringLiteral("network");
    case AppErrorCode::RateLimit: return QStringLiteral("rate_limit");
    case AppErrorCode::InvalidRequest: return QStringLiteral("invalid_request");
    case AppErrorCode::InvalidResponse: return QStringLiteral("invalid_response");
    case AppErrorCode::Cancelled: return QStringLiteral("cancelled");
    case AppErrorCode::Unsupported: return QStringLiteral("unsupported");
    case AppErrorCode::Unknown: return QStringLiteral("unknown");
    }
    return QStringLiteral("unknown");
}

QString AppError::domainName() const
{
    switch (domain) {
    case ErrorDomain::None: return QStringLiteral("none");
    case ErrorDomain::Configuration: return QStringLiteral("configuration");
    case ErrorDomain::Infrastructure: return QStringLiteral("infrastructure");
    case ErrorDomain::Database: return QStringLiteral("database");
    case ErrorDomain::Network: return QStringLiteral("network");
    case ErrorDomain::Model: return QStringLiteral("model");
    case ErrorDomain::Memory: return QStringLiteral("memory");
    case ErrorDomain::Application: return QStringLiteral("application");
    }
    return QStringLiteral("unknown");
}

} // namespace zhu_screen_pet
