#pragma once

#include <QPushButton>
#include <QIcon>

class DockTip;

// 任务管理器中的单个图标按钮：自绘 32x32 图标 + 底部活动指示条
class TaskButton : public QPushButton
{
    Q_OBJECT
public:
    explicit TaskButton(QWidget *parent = nullptr);

    void setAppIcon(const QIcon &icon);
    void setActive(bool active);
    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent *e) override;
    void enterEvent(QEnterEvent *e) override;
    void leaveEvent(QEvent *e) override;
    void hideEvent(QHideEvent *e) override;

private:
    DockTip *ensureTip();

    QIcon m_icon;
    bool m_active = false;
    bool m_hover = false;
    DockTip *m_tip = nullptr;
};
