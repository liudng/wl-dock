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

    QFont f = font();
    f.setPointSize(10);
    setFont(f);

    const QFontMetrics fm(f);
    const int padX = 10;
    const int padY = 5;

    // anchor 局部坐标 → dock（共同祖先）坐标，水平居中贴在按钮上方
    QWidget *dock = anchor->window();

    // 文本过宽时省略：若 tooltip 宽于 dock 窗口，qBound 的上界
    // dock->width() - w 会变负，触发 Q_ASSERT(!(max < min)) 崩溃。
    // 注意：宽度恰好等于文本 advance 时 elidedText 也会因内部度量
    // 差异误加省略号，因此只在真正超宽时才走省略路径。
    int textW = fm.horizontalAdvance(text);
    m_text = text;
    if (dock) {
        const int avail = qMax(0, dock->width() - padX * 2);
        if (textW > avail) {
            textW = avail;
            m_text = fm.elidedText(text, Qt::ElideRight, avail);
        }
    }

    const int w = textW + padX * 2;
    const int h = fm.height() + padY * 2;
    resize(w, h);

    const QPoint anchorTopCenter = anchor->mapTo(dock, QPoint(anchor->width() / 2, 0));

    int x = anchorTopCenter.x() - w / 2;
    int y = anchorTopCenter.y() - h - 4; // 上方 4px 间隙

    // 不超出 dock 左右边界
    if (dock) {
        x = qBound(0, x, qMax(0, dock->width() - w));
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
