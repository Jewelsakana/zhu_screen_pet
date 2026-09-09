#pragma once

namespace zhu_screen_pet {

/** 截图功能跨配置、UI 与基础设施共享的业务边界。 */
struct ScreenCapturePolicy final
{
    static constexpr int MinimumAutomaticIntervalMs = 30000;
    static constexpr int MaximumAutomaticIntervalMs = 600000;
    static constexpr int RecommendedAutomaticIntervalMs = 60000;
    /** 自动视觉请求允许同时排队的最大数量；超出后直接丢弃，不积压截图。 */
    static constexpr int MaximumBackgroundQueueSize = 1;
    /** 连续视觉请求失败达到该次数后暂停自动截图。 */
    static constexpr int AutomaticFailureCircuitThreshold = 3;
    /** 自动截图熔断后的冷却时间。 */
    static constexpr int AutomaticFailureCooldownMs = 5 * 60 * 1000;
};

} // namespace zhu_screen_pet
