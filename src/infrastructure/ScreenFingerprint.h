#pragma once

#include <QByteArray>

class QImage;

namespace zhu_screen_pet {

/** 屏幕画面的紧凑差分哈希；仅保留结构摘要，不持有或持久化原始截图。 */
class ScreenFingerprint final
{
public:
    static constexpr int Width = 64;
    static constexpr int Height = 36;
    static constexpr double ChangeThreshold = 0.03;

    /** 生成 Width * Height 位的横向差分哈希，当前大小为 288 字节。 */
    static QByteArray create(const QImage& image);
    /** 返回两个同规格摘要中不同位的比例；无效或规格不同时视为完全变化。 */
    static double differenceRatio(const QByteArray& previous,
                                  const QByteArray& current);
    static bool hasSignificantChange(const QByteArray& previous,
                                     const QByteArray& current,
                                     double threshold = ChangeThreshold);
};

} // namespace zhu_screen_pet
