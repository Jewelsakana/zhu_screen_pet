#pragma once

#include "core/AppError.h"

namespace zhu_screen_pet {

/** 模型层对外暴露的稳定错误分类。 */
using ModelErrorCode = AppErrorCode;

/** 一次模型请求的错误详情。 */
using ModelError = AppError;

} // namespace zhu_screen_pet
