#include "SniIconButton.h"
#include "SniItem.h"
#include "../DockTip.h"

#include <QPainter>
#include <QEnterEvent>
#include <QHideEvent>
#include <QMouseEvent>

SniIconButton::SniIconButton(SniItem *item, QWidget *parent)
    : QPushButton(parent)
    , m_item(item)
{
    setFixedSize(44, 44);
    setAttribute(Qt::WA_TranslucentBackground);
    setFocusPolicy(Qt::NoFocus);
    setCursor(Qt::PointingHandCursor);
    setMouseTracking(true);
}

void SniIconButton::setTrayIcon(const QIcon &icon)
{
    m_icon = icon;
    update();
}

void SniIconButton::setAttention(bool attention)
{
    if (m_attention == attention) return;
    m_attention = attention;
    update();
}

QSize SniIconButton::sizeHint() const
{
    return {44, 44};
}

void SniIconButton::paintEvent(QPaintEvent *e)
{
    Q_UNUSED(e);
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    // hover 背景（与 TaskButton 一致）
    if (m_hover || underMouse()) {
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(255, 255, 255, 30));
        p.drawRoundedRect(rect().adjusted(2, 2, -2, -2), 6, 6);
    }

    // 32×32 图标居中（与 TaskButton 一致）
    const QRect iconRect((width() - 32) / 2, (height() - 32) / 2 - 2, 32, 32);
    if (!m_icon.isNull()) {
        m_icon.paint(&p, iconRect, Qt::AlignCenter, QIcon::Normal, QIcon::On);
    } else {
        // 无图标时画一个占位（与 TaskButton 一致）
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(80, 80, 90));
        p.drawRoundedRect(iconRect, 4, 4);
    }

    // NeedsAttention 指示：底部橙色短横线（区别于 TaskButton 的蓝色 active 指示）
    if (m_attention) {
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(255, 160, 0));
        p.drawRoundedRect(QRect(width() / 2 - 10, height() - 5, 20, 3), 2, 2);
    }
}

void SniIconButton::enterEvent(QEnterEvent *e)
{
    Q_UNUSED(e);
    m_hover = true;
    update();
    // 立即显示 tooltip（复用 DockTip，与 TaskButton 一致）
    ensureTip()->showTip(this, toolTip());
}

void SniIconButton::leaveEvent(QEvent *e)
{
    Q_UNUSED(e);
    m_hover = false;
    update();
    if (m_tip)
        m_tip->hideTip();
}

void SniIconButton::hideEvent(QHideEvent *e)
{
    QPushButton::hideEvent(e);
    if (m_tip)
        m_tip->hideTip();
}

void SniIconButton::mousePressEvent(QMouseEvent *e)
{
    QPushButton::mousePressEvent(e);
    if (e->button() == Qt::LeftButton)
        emit activateRequested(m_item);
    else if (e->button() == Qt::MiddleButton)
        emit secondaryActivateRequested(m_item);
}

DockTip *SniIconButton::ensureTip()
{
    if (!m_tip) {
        QWidget *root = window();
        m_tip = new DockTip(root);
    }
    return m_tip;
}
