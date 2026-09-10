#pragma once

#include <QObject>
#include <QPixmap>
#include <QVector>

#include "app/PetState.h"

class QLabel;
class QTimer;

namespace zhu_screen_pet {

/** 在 QLabel 上播放按状态组织的 PNG 序列，素材缺失时回退到静态头像。 */
class PetAnimationPlayer final : public QObject
{
    Q_OBJECT

public:
    explicit PetAnimationPlayer(QLabel* target, QObject* parent = nullptr);

    void setFallbackPixmap(const QPixmap& pixmap);
    void setAnimationRoot(const QString& directory);
    void setTargetSize(const QSize& size);
    void setState(PetState state);
    void playLevelUp();
    void stopLevelUp();

    int frameCount(PetState state) const;
    int levelUpFrameCount() const;

private:
    QVector<QPixmap> loadFrames(const QString& stateDirectory) const;
    const QVector<QPixmap>& framesFor(PetState state) const;
    void startLoop(const QVector<QPixmap>& frames, int intervalMs);
    void scheduleIdleBlink();
    void advanceFrame();
    void showPixmap(const QPixmap& pixmap);
    void showFallback();

    QLabel* target_ = nullptr;
    QTimer* frameTimer_ = nullptr;
    QTimer* idleTimer_ = nullptr;
    QPixmap fallbackPixmap_;
    QPixmap currentPixmap_;
    QVector<QPixmap> idleFrames_;
    QVector<QPixmap> thinkingFrames_;
    QVector<QPixmap> speakingFrames_;
    QVector<QPixmap> levelUpFrames_;
    const QVector<QPixmap>* playingFrames_ = nullptr;
    QSize targetSize_;
    PetState state_ = PetState::Idle;
    int frameIndex_ = 0;
    bool levelUpActive_ = false;
    bool idleBlinkActive_ = false;
};

} // namespace zhu_screen_pet
