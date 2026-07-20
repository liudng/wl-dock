#pragma once

#include <QPushButton>
#include <QIcon>

class DockTip;
class SniItem;

// 系统托盘图标按钮：视觉风格照搬 TaskButton（44×44 / 32×32 图标 / hover 圆角背景），
// 但语义不同：TaskButton 激活窗口，SniIconButton 触发 SNI 应用动作。
// 底部指示条表示 NeedsAttention 状态（而非 active 窗口）。
// tooltip 复用 DockTip。
class SniIconButton : public QPushButton
{
    Q_OBJECT
public:
    explicit SniIconButton(SniItem *item, QWidget *parent = nullptr);

    void setTrayIcon(const QIcon &icon);
    void setAttention(bool attention);
    QSize sizeHint() const override;

    SniItem *item() const { return m_item; }

signals:
    void activateRequested(SniItem *item);
    void secondaryActivateRequested(SniItem *item);
    // 右键请求，或 ItemIsMenu=true 的左键请求：走 SNI ContextMenu
    // globalPos 是鼠标的全局位置，host 弹 QMenu 用
    void contextMenuRequested(SniItem *item, const QPoint &globalPos);

protected:
    void paintEvent(QPaintEvent *e) override;
    void enterEvent(QEnterEvent *e) override;
    void leaveEvent(QEvent *e) override;
    void hideEvent(QHideEvent *e) override;
    void mousePressEvent(QMouseEvent *e) override;

private:
    DockTip *ensureTip();

    SniItem *m_item;
    QIcon m_icon;
    bool m_hover = false;
    bool m_attention = false;
    DockTip *m_tip = nullptr;
};
