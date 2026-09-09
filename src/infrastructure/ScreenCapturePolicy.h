#pragma once

namespace zhu_screen_pet {

/** 截图功能跨配置、UI 与基础设施共享的业务边界。 */
struct ScreenCapturePolicy final
{
    static constexpr int MinimumAutomaticIntervalMs = 30000;
    static constexpr int MaximumAutomaticIntervalMs = 600000;
    static constexpr int RecommendedAutomaticIntervalMs = 60000;
};

} // namespace zhu_screen_pet
