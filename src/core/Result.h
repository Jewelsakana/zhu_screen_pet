#pragma once

#include <utility>

#include "core/AppError.h"

namespace zhu_screen_pet {

/** 统一同步操作结果；成功时 value 有效，失败时 error 携带稳定分类和技术详情。 */
template <typename T>
class Result final
{
public:
    static Result success(T value)
    {
        Result result;
        result.ok_ = true;
        result.value_ = std::move(value);
        return result;
    }

    static Result failure(AppError error)
    {
        Result result;
        result.error_ = std::move(error);
        return result;
    }

    bool succeeded() const { return ok_; }
    explicit operator bool() const { return succeeded(); }
    const T& value() const { return value_; }
    T& value() { return value_; }
    const AppError& error() const { return error_; }

private:
    bool ok_ = false;
    T value_{};
    AppError error_{};
};

/** 无返回值操作的统一结果。 */
template <>
class Result<void> final
{
public:
    static Result success()
    {
        Result result;
        result.ok_ = true;
        return result;
    }

    static Result failure(AppError error)
    {
        Result result;
        result.error_ = std::move(error);
        return result;
    }

    bool succeeded() const { return ok_; }
    explicit operator bool() const { return succeeded(); }
    const AppError& error() const { return error_; }

private:
    bool ok_ = false;
    AppError error_{};
};

} // namespace zhu_screen_pet
