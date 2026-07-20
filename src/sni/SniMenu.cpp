#include "SniMenu.h"

#include <QPainter>
#include <QPaintEvent>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QHideEvent>
#include <QEventLoop>
#include <QApplication>
#include <QGuiApplication>
#include <QScreen>
#include <QFontMetrics>
#include <QWindow>
#include <QEvent>
#include <QLoggingCategory>
#include <LayerShellQt/Window>

SniMenu::SniMenu(QWidget *parent)
    : QWidget(parent)
{
    // Qt::Window 让 LayerShellQt 当作普通 layer_surface attach（不会触发
    // popup attach 失败路径），从而能通过 setMargins 精确定位。
    // Qt::ToolTip / Qt::Popup 都会被 LayerShellQt 当作 popup attach 失败。
    setWindowFlags(Qt::Window | Qt::FramelessWindowHint);
    setAttribute(Qt::WA_ShowWithoutActivating, false);
    setAttribute(Qt::WA_DeleteOnClose, false);
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);
}

void SniMenu::setItems(const QList<Item> &items)
{
    m_items = items;
    m_hovered = -1;
    resize(sizeHint());
}

QSize SniMenu::sizeHint() const
{
    int w = 80;
    int h = 4;
    const QFontMetrics fm(font());
    for (const auto &item : m_items) {
        if (item.separator) {
            h += 9;
        } else {
            h += fm.height() + 10;
            int iw = fm.horizontalAdvance(item.label) + 56; // icon + checkmark + padding
            if (!item.icon.isNull())
                iw += 20;
            w = std::max(w, iw);
        }
    }
    return QSize(w, std::max(h, 20));
}

int SniMenu::itemTop(int index) const
{
    int top = 4;
    const QFontMetrics fm(font());
    for (int i = 0; i < index; ++i) {
        if (m_items[i].separator)
            top += 9;
        else
            top += fm.height() + 10;
    }
    return top;
}

int SniMenu::itemHeight(int index) const
{
    if (m_items[index].separator)
        return 9;
    return QFontMetrics(font()).height() + 10;
}

int SniMenu::itemAtY(int y) const
{
    for (int i = 0; i < m_items.size(); ++i) {
        const int top = itemTop(i);
        if (y >= top && y < top + itemHeight(i))
            return i;
    }
    return -1;
}

void SniMenu::updateHover(int y)
{
    const int newHover = itemAtY(y);
    if (newHover != m_hovered) {
        m_hovered = newHover;
        update();
    }
}

void SniMenu::moveHover(int delta)
{
    if (m_items.isEmpty())
        return;
    int idx = m_hovered;
    if (idx < 0) {
        idx = delta > 0 ? -1 : m_items.size();
    }
    for (int steps = 0; steps < m_items.size(); ++steps) {
        idx += delta;
        if (idx < 0)
            idx = m_items.size() - 1;
        else if (idx >= m_items.size())
            idx = 0;
        if (!m_items[idx].separator && m_items[idx].enabled) {
            m_hovered = idx;
            update();
            return;
        }
    }
}

void SniMenu::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, false);

    // 背景
    p.fillRect(rect(), QColor(32, 32, 32, 240));
    p.setPen(QColor(70, 70, 70));
    p.drawRect(rect().adjusted(0, 0, -1, -1));

    const QFontMetrics fm(font());
    const int iconSize = 16;

    for (int i = 0; i < m_items.size(); ++i) {
        const Item &item = m_items[i];
        const QRect r(1, itemTop(i), width() - 2, itemHeight(i));

        if (item.separator) {
            p.setPen(QColor(70, 70, 70));
            p.drawLine(r.x() + 8, r.y() + r.height() / 2, r.right() - 8, r.y() + r.height() / 2);
            continue;
        }

        if (i == m_hovered && item.enabled) {
            p.fillRect(r, QColor(56, 116, 220));
        }

        p.setPen(item.enabled ? QColor(220, 220, 220) : QColor(120, 120, 120));

        const int textY = r.y() + (r.height() + fm.ascent() - fm.descent()) / 2;
        int x = r.x() + 10;

        if (item.checkable) {
            if (item.checked)
                p.drawText(x, textY, QStringLiteral("\u2713")); // ✓
            x += 18;
        }

        if (!item.icon.isNull()) {
            item.icon.paint(&p, x, r.y() + (r.height() - iconSize) / 2, iconSize, iconSize);
            x += iconSize + 6;
        }

        p.drawText(x, textY, item.label);
    }
}

void SniMenu::mouseMoveEvent(QMouseEvent *e)
{
    updateHover(static_cast<int>(e->position().y()));
}

void SniMenu::mousePressEvent(QMouseEvent *e)
{
    if (e->button() != Qt::LeftButton) {
        hide();
        return;
    }
    const int idx = itemAtY(static_cast<int>(e->position().y()));
    if (idx < 0 || m_items[idx].separator || !m_items[idx].enabled) {
        hide();
        return;
    }
    m_result = m_items[idx].id;
    hide();
}

void SniMenu::keyPressEvent(QKeyEvent *e)
{
    switch (e->key()) {
    case Qt::Key_Escape:
        hide();
        break;
    case Qt::Key_Up:
        moveHover(-1);
        break;
    case Qt::Key_Down:
        moveHover(1);
        break;
    case Qt::Key_Return:
    case Qt::Key_Enter:
        if (m_hovered >= 0 && !m_items[m_hovered].separator && m_items[m_hovered].enabled) {
            m_result = m_items[m_hovered].id;
            hide();
        }
        break;
    default:
        QWidget::keyPressEvent(e);
    }
}

void SniMenu::hideEvent(QHideEvent *)
{
    if (m_loop)
        m_loop->quit();
}

bool SniMenu::eventFilter(QObject *o, QEvent *e)
{
    // 外部点击 / 按键关闭菜单
    if (o != this) {
        if (e->type() == QEvent::MouseButtonPress
            || e->type() == QEvent::Wheel) {
            hide();
        }
    }
    return QWidget::eventFilter(o, e);
}

static Q_LOGGING_CATEGORY(logSniMenuPopup, "dock.sni.menu.popup", QtWarningMsg)

int SniMenu::popup(QWindow *dockWindow, const QPoint &globalPos)
{
    m_result = -1;
    m_hovered = -1;

    const QSize sz = sizeHint();
    resize(sz);

    // globalPos 是 Qt 虚拟桌面全局坐标。QPlatformWindow::mapToGlobal(pos) = pos +
    // geometry().topLeft()，Qt Wayland 把 dock window 的 geometry topLeft 设置为
    // screen geometry topLeft（多屏下每个 dock window 的 geometry topLeft 对应其
    // 所在 screen 的 geometry topLeft），所以 globalPos 是真正的全局坐标。
    //
    // LayerShellQt 的 setMargins 是相对 output 左上角的局部坐标，不是全局坐标。
    // 需要把 globalPos 转换为相对 output 左上角的局部坐标：
    //   localX = globalPos.x - screen.geometry.left()
    //   localY = globalPos.y - screen.geometry.top() + dockY
    // 其中 dockY = screenH - dockH 补偿 dock surface 顶部在 output 中的 Y 偏移
    // （dock 用 AnchorBottom，surface 顶部 Y = screenH - dockH）。
    QScreen *screen = dockWindow ? dockWindow->screen() : QGuiApplication::primaryScreen();
    int screenH = screen ? screen->geometry().height() : 1080;
    int dockH = dockWindow ? dockWindow->height() : 64;
    if (dockH <= 1)
        dockH = 64; // 隐藏状态用默认激活高度
    const int dockY = screenH - dockH;

    const QRect sg = screen ? screen->geometry() : QRect(0, 0, 0, 0);
    QPoint pos(globalPos.x() - sg.left(), globalPos.y() - sg.top() + dockY);

    qCWarning(logSniMenuPopup) << "popup: dockWindow=" << dockWindow
                               << "dockWindow->screen()="
                               << (dockWindow && dockWindow->screen() ? dockWindow->screen()->name() : QStringLiteral("null"))
                               << "globalPos=" << globalPos << "dockH=" << dockH
                               << "screenH=" << screenH << "dockY=" << dockY
                               << "pos=" << pos
                               << "screen->geometry=" << (screen ? screen->geometry() : QRect());

    // 确保不超出屏幕（pos 是相对 output 左上角的局部坐标）
    if (screen) {
        const int sw = screen->geometry().width();
        const int sh = screen->geometry().height();
        if (pos.x() + sz.width() > sw)
            pos.setX(sw - sz.width());
        if (pos.y() + sz.height() > sh)
            pos.setY(sh - sz.height());
        if (pos.x() < 0)
            pos.setX(0);
        if (pos.y() < 0)
            pos.setY(0);
    }

    // 通过 LayerShellQt::Window 用 anchor + margin 精确定位。
    // anchors=None 在多数 compositor（含 Labwc）下让 layer_surface 居中显示，
    // 此时 margin 不生效。改用 AnchorTop | AnchorLeft（左上角对齐，不拉伸），
    // 再用 setMargins(left, top, 0, 0) 偏移到 (pos.x, pos.y)。
    //
    // 多屏支持：关键在于让 layer_surface attach 到 dock 所在 output。
    // QWaylandLayerSurface 构造函数读取 QWindow::screen()->handle() 决定 wl_output，
    // 而 QWindow::screen() 返回 topLevelScreen。topLevelScreen 的值受两处影响：
    //   1. QWidgetPrivate::create() (qwidget.cpp:1327) 读取 initialScreen 调
    //      QWindow::setScreen()，设置 topLevelScreen = 副屏。
    //   2. QWindowPrivate::create() (qwindow.cpp:545) 调用 screenForGeometry(geometry)
    //      根据窗口 geometry 中心点查找屏幕，若中心点不在当前 screen 内，会遍历
    //      virtualSiblings 覆盖 topLevelScreen。默认 geometry (0,0,w,h) 中心点
    //      在主屏，topLevelScreen 被覆盖回主屏 → layer_surface attach 到主屏 output
    //      → margin 用的是副屏坐标 → 菜单显示在主屏左上角。
    //
    // 解决方案：winId() 之前 setGeometry 到目标屏幕区域，让 screenForGeometry
    // 直接返回目标屏幕，不覆盖 topLevelScreen。Wayland 下 hasCapability(WindowManagement)
    // = false，QWidgetPrivate::create() 会调 win->setGeometry(q->geometry())
    // (qwidget.cpp:1317)，所以 setGeometry 在 winId() 前有效。
    // LayerShellQt 用 anchor + margin 定位，setGeometry 不影响最终位置。
    if (dockWindow && dockWindow->screen()) {
        setScreen(dockWindow->screen());
        QScreen *targetScreen = dockWindow->screen();
        setGeometry(QRect(targetScreen->geometry().topLeft(), sz));
    }
    qCWarning(logSniMenuPopup) << "before winId: windowHandle->screen()="
                               << (windowHandle() && windowHandle()->screen() ? windowHandle()->screen()->name() : QStringLiteral("null"));
    winId();
    qCWarning(logSniMenuPopup) << "after winId: windowHandle->screen()="
                               << (windowHandle() && windowHandle()->screen() ? windowHandle()->screen()->name() : QStringLiteral("null"));
    // 兜底：若 screenForGeometry 仍然覆盖了 topLevelScreen，再修正一次。
    // windowRecreationRequired() 对 virtual siblings 返回 false，不会重建平台窗口，
    // 只是 connectToScreen(newScreen) 设置 topLevelScreen = newScreen。
    if (dockWindow && dockWindow->screen() && windowHandle()
        && windowHandle()->screen() != dockWindow->screen()) {
        qCWarning(logSniMenuPopup) << "screen mismatch after winId, applying fallback setScreen";
        windowHandle()->setScreen(dockWindow->screen());
    }
    qCWarning(logSniMenuPopup) << "after setScreen: windowHandle->screen()="
                               << (windowHandle() && windowHandle()->screen() ? windowHandle()->screen()->name() : QStringLiteral("null"));
    if (auto *lw = LayerShellQt::Window::get(windowHandle())) {
        lw->setLayer(LayerShellQt::Window::LayerOverlay);
        lw->setAnchors(LayerShellQt::Window::Anchors(LayerShellQt::Window::AnchorTop | LayerShellQt::Window::AnchorLeft));
        lw->setKeyboardInteractivity(LayerShellQt::Window::KeyboardInteractivityExclusive);
        lw->setMargins(QMargins(pos.x(), pos.y(), 0, 0));
    }
    qCWarning(logSniMenuPopup) << "before show: windowHandle->screen()="
                               << (windowHandle() && windowHandle()->screen() ? windowHandle()->screen()->name() : QStringLiteral("null"));

    show();
    raise();
    activateWindow();
    setFocus();

    qApp->installEventFilter(this);

    QEventLoop loop;
    m_loop = &loop;
    loop.exec();
    m_loop = nullptr;

    qApp->removeEventFilter(this);

    return m_result;
}
