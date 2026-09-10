#include "ui/PetAnimationPlayer.h"

#include <QDir>
#include <QFileInfoList>
#include <QLabel>
#include <QRandomGenerator>
#include <QTimer>

#include <utility>

namespace zhu_screen_pet {

PetAnimationPlayer::PetAnimationPlayer(QLabel* target, QObject* parent)
    : QObject(parent), target_(target), frameTimer_(new QTimer(this)),
      idleTimer_(new QTimer(this))
{
    frameTimer_->setTimerType(Qt::CoarseTimer);
    idleTimer_->setSingleShot(true);
    idleTimer_->setTimerType(Qt::VeryCoarseTimer);
    connect(frameTimer_, &QTimer::timeout, this, &PetAnimationPlayer::advanceFrame);
    connect(idleTimer_, &QTimer::timeout, this, [this]() {
        if (state_ != PetState::Idle || levelUpActive_ || idleFrames_.isEmpty()) return;
        idleBlinkActive_ = true;
        startLoop(idleFrames_, 120);
    });
}

void PetAnimationPlayer::setFallbackPixmap(const QPixmap& pixmap)
{
    fallbackPixmap_ = pixmap;
    if (playingFrames_ == nullptr) showFallback();
}

void PetAnimationPlayer::setAnimationRoot(const QString& directory)
{
    const QDir root(directory);
    idleFrames_ = loadFrames(root.filePath(QStringLiteral("idle")));
    thinkingFrames_ = loadFrames(root.filePath(QStringLiteral("thinking")));
    speakingFrames_ = loadFrames(root.filePath(QStringLiteral("speaking")));
    levelUpFrames_ = loadFrames(root.filePath(QStringLiteral("level_up")));
    if (levelUpActive_) playLevelUp();
    else setState(state_);
}

void PetAnimationPlayer::setTargetSize(const QSize& size)
{
    targetSize_ = size;
    if (!currentPixmap_.isNull()) showPixmap(currentPixmap_);
}

void PetAnimationPlayer::setState(PetState state)
{
    state_ = state;
    if (levelUpActive_) return;
    frameTimer_->stop();
    idleTimer_->stop();
    playingFrames_ = nullptr;
    idleBlinkActive_ = false;
    frameIndex_ = 0;
    showFallback();
    if (state == PetState::Idle) {
        scheduleIdleBlink();
        return;
    }
    const QVector<QPixmap>& frames = framesFor(state);
    if (frames.isEmpty()) return;
    const int interval = state == PetState::Thinking ? 400
        : state == PetState::Speaking ? 220 : 400;
    startLoop(frames, interval);
}

void PetAnimationPlayer::playLevelUp()
{
    levelUpActive_ = true;
    idleTimer_->stop();
    frameTimer_->stop();
    playingFrames_ = nullptr;
    idleBlinkActive_ = false;
    frameIndex_ = 0;
    if (levelUpFrames_.isEmpty()) {
        showFallback();
        return;
    }
    startLoop(levelUpFrames_, 250);
}

void PetAnimationPlayer::stopLevelUp()
{
    if (!levelUpActive_) return;
    levelUpActive_ = false;
    setState(state_);
}

int PetAnimationPlayer::frameCount(PetState state) const
{
    return framesFor(state).size();
}

int PetAnimationPlayer::levelUpFrameCount() const
{
    return levelUpFrames_.size();
}

QVector<QPixmap> PetAnimationPlayer::loadFrames(const QString& stateDirectory) const
{
    QVector<QPixmap> frames;
    const QDir directory(stateDirectory);
    const QFileInfoList files = directory.entryInfoList(
        {QStringLiteral("*.png")}, QDir::Files | QDir::Readable, QDir::Name | QDir::IgnoreCase);
    frames.reserve(files.size());
    for (const QFileInfo& file : files) {
        QPixmap frame(file.absoluteFilePath());
        if (!frame.isNull()) frames.append(std::move(frame));
    }
    return frames;
}

const QVector<QPixmap>& PetAnimationPlayer::framesFor(PetState state) const
{
    switch (state) {
    case PetState::Idle: return idleFrames_;
    case PetState::Thinking: return thinkingFrames_;
    case PetState::Speaking: return speakingFrames_;
    case PetState::Error: break;
    }
    static const QVector<QPixmap> empty;
    return empty;
}

void PetAnimationPlayer::startLoop(const QVector<QPixmap>& frames, int intervalMs)
{
    if (frames.isEmpty()) return;
    playingFrames_ = &frames;
    frameIndex_ = 0;
    showPixmap(frames.constFirst());
    frameTimer_->start(intervalMs);
}

void PetAnimationPlayer::scheduleIdleBlink()
{
    if (idleFrames_.isEmpty()) return;
    const int delayMs = QRandomGenerator::global()->bounded(3500, 6501);
    idleTimer_->start(delayMs);
}

void PetAnimationPlayer::advanceFrame()
{
    if (playingFrames_ == nullptr || playingFrames_->isEmpty()) {
        frameTimer_->stop();
        return;
    }
    ++frameIndex_;
    if (idleBlinkActive_ && frameIndex_ >= playingFrames_->size()) {
        frameTimer_->stop();
        playingFrames_ = nullptr;
        idleBlinkActive_ = false;
        showFallback();
        scheduleIdleBlink();
        return;
    }
    frameIndex_ %= playingFrames_->size();
    showPixmap(playingFrames_->at(frameIndex_));
}

void PetAnimationPlayer::showPixmap(const QPixmap& pixmap)
{
    currentPixmap_ = pixmap;
    if (target_ == nullptr || pixmap.isNull()) return;
    QSize size = targetSize_;
    if (!size.isValid() || size.isEmpty()) size = target_->size();
    if (!size.isValid() || size.isEmpty()) return;
    target_->setPixmap(pixmap.scaled(size, Qt::KeepAspectRatio,
                                     Qt::SmoothTransformation));
}

void PetAnimationPlayer::showFallback()
{
    if (!fallbackPixmap_.isNull()) showPixmap(fallbackPixmap_);
}

} // namespace zhu_screen_pet
