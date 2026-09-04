#pragma once

#include <QMetaType>
#include <QString>

namespace zhu_screen_pet {

/** 错误所属领域，用于日志、重试策略和用户提示路由。 */
enum class ErrorDomain
{
    None,
    Configuration,
    Infrastructure,
    Database,
    Network,
    Model,
    Memory,
    Application
};

/** 跨层稳定错误码；底层技术文本不应直接展示给用户。 */
enum class AppErrorCode
{
    None,
    InvalidArgument,
    NotFound,
    Busy,
    NotReady,
    ConfigInvalid,
    Io,
    DatabaseUnavailable,
    DatabaseQuery,
    Authentication,
    Timeout,
    Network,
    RateLimit,
    InvalidRequest,
    InvalidResponse,
    Cancelled,
    Unsupported,
    Unknown
};

/** 所有层统一传递的结构化错误。前三个字段保持兼容旧 ModelError 聚合初始化。 */
struct AppError
{
    AppErrorCode code = AppErrorCode::None;
    QString message;
    int httpStatus = 0;
    ErrorDomain domain = ErrorDomain::None;
    QString technicalMessage;
    QString operation;
    QString correlationId;
    bool retryable = false;
    int nativeCode = 0;

    QString codeName() const;
    QString domainName() const;
    bool isFailure() const { return code != AppErrorCode::None; }
};

Q_DECLARE_METATYPE(zhu_screen_pet::AppError)

} // namespace zhu_screen_pet
