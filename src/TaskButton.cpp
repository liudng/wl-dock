#include "TaskButton.h"

#include <QPainter>
#include <QEnterEvent>

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
}

void TaskButton::leaveEvent(QEvent *e)
{
    Q_UNUSED(e);
    m_hover = false;
    update();
}
