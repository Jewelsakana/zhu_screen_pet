#pragma once

#include <QWidget>

class QPushButton;
class QTextEdit;

namespace zhu_screen_pet {

/** 桌宠下方的快速聊天输入面板。 */
class ChatInputPanel final : public QWidget
{
    Q_OBJECT

public:
    explicit ChatInputPanel(QWidget* parent = nullptr);

    QString text() const;
    void clear();
    void focusInput();
    void setBusy(bool busy);
    void setRetryEnabled(bool enabled);
    bool canAutoHide() const;

signals:
    void sendRequested();
    void cancelRequested();
    void retryRequested();

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    QTextEdit* input_ = nullptr;
    QPushButton* send_ = nullptr;
    QPushButton* cancel_ = nullptr;
    QPushButton* retry_ = nullptr;
    bool composing_ = false;
};

} // namespace zhu_screen_pet
