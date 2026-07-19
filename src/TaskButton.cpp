#include "TaskButton.h"
#include "DockTip.h"

#include <QPainter>
#include <QEnterEvent>
#include <QHideEvent>

TaskButton::TaskButton(QWidget *parent)
    : QPushButton(parent)
{
    setFixedSize(44, 44);
    setAttribute(Qt::WA_TranslucentBackground);
    setFocusPolicy(Qt::NoFocus);
    setCursor(Qt::PointingHandCursor);
    setMouseTracking(true);
}

void TaskButton::setAppIcon(const QIcon &icon)
{
    m_icon = icon;
    update();
}

void TaskButton::setActive(bool a)
{
    if (m_active == a) return;
    m_active = a;
    update();
}

QSize TaskButton::sizeHint() const
{
    return {44, 44};
}

void TaskButton::paintEvent(QPaintEvent *e)
{
    Q_UNUSED(e);
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    // hover 背景
    if (m_hover || underMouse()) {
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(255, 255, 255, 30));
        p.drawRoundedRect(rect().adjusted(2, 2, -2, -2), 6, 6);
    }

    // 32x32 图标居中
    const QRect iconRect((width() - 32) / 2, (height() - 32) / 2 - 2, 32, 32);
    if (!m_icon.isNull()) {
        m_icon.paint(&p, iconRect, Qt::AlignCenter, QIcon::Normal, QIcon::On);
    } else {
        p.setPen(QColor(220, 220, 220));
        p.setBrush(QColor(80, 80, 90));
        p.drawRoundedRect(iconRect, 4, 4);
        p.setPen(QColor(220, 220, 220));
        QFont f = p.font();
        f.setPointSize(10);
        f.setBold(true);
        p.setFont(f);
        p.drawText(iconRect, Qt::AlignCenter, QStringLiteral("?"));
    }

    // 活动窗口指示：底部强调色短横线
    if (m_active) {
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(80, 160, 255));
        p.drawRoundedRect(QRect(width() / 2 - 10, height() - 5, 20, 3), 2, 2);
    }
}

void TaskButton::enterEvent(QEnterEvent *e)
{
    Q_UNUSED(e);
    m_hover = true;
    update();
    ensureTip()->showTip(this, toolTip());
}

void TaskButton::leaveEvent(QEvent *e)
{
    Q_UNUSED(e);
    m_hover = false;
    update();
    if (m_tip)
        m_tip->hideTip();
}

void TaskButton::hideEvent(QHideEvent *e)
{
    QPushButton::hideEvent(e);
    // dock 自动隐藏时按钮被隐藏，确保 tooltip 跟着消失
    if (m_tip)
        m_tip->hideTip();
}

DockTip *TaskButton::ensureTip()
{
    // tip 作为 dock（共同祖先窗口）的子控件，而不是按钮的子控件，
    // 否则它会受按钮局部坐标变换影响，且无法越过按钮边界绘制。
    if (!m_tip) {
        QWidget *root = window();
        m_tip = new DockTip(root);
    }
    return m_tip;
}
