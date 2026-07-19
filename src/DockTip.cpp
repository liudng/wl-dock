#include "DockTip.h"

#include <QPainter>
#include <QPainterPath>

DockTip::DockTip(QWidget *parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_TransparentForMouseEvents); // 鼠标事件穿透：不触发额外 enter/leave
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_NoSystemBackground);
    setFocusPolicy(Qt::NoFocus);
    hide();
}

void DockTip::showTip(QWidget *anchor, const QString &text)
{
    if (!anchor || text.isEmpty()) {
        hideTip();
        return;
    }

    m_text = text;

    QFont f = font();
    f.setPointSize(10);
    setFont(f);

    const QFontMetrics fm(f);
    const int padX = 10;
    const int padY = 5;
    const int textW = fm.horizontalAdvance(text);
    const int textH = fm.height();
    const int w = textW + padX * 2;
    const int h = textH + padY * 2;
    resize(w, h);

    // anchor 局部坐标 → dock（共同祖先）坐标，水平居中贴在按钮上方
    QWidget *dock = anchor->window();
    const QPoint anchorTopCenter = anchor->mapTo(dock, QPoint(anchor->width() / 2, 0));

    int x = anchorTopCenter.x() - w / 2;
    int y = anchorTopCenter.y() - h - 4; // 上方 4px 间隙

    // 不超出 dock 左右边界
    if (dock) {
        x = qBound(0, x, dock->width() - w);
        // 上方放不下就退到 anchor 下方
        if (y < 0)
            y = anchor->mapTo(dock, QPoint(0, anchor->height())).y() + 4;
    }
    move(x, y);

    raise();
    show();
    update();
}

void DockTip::hideTip()
{
    hide();
    m_text.clear();
}

void DockTip::paintEvent(QPaintEvent *e)
{
    Q_UNUSED(e);
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    QPainterPath path;
    path.addRoundedRect(rect(), 6, 6);
    p.fillPath(path, QColor(20, 20, 24, 220));

    p.setPen(QColor(220, 220, 220));
    p.drawText(rect(), Qt::AlignCenter, m_text);
}
