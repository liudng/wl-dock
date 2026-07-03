#include "DockWindow.h"

#include "TaskManager.h"
#include "ClockWidget.h"
#include "ForeignToplevelManager.h"
#include "DesktopIconResolver.h"

#include <LayerShellQt/Window>

#include <QHBoxLayout>
#include <QPainter>
#include <QPainterPath>
#include <QScreen>
#include <QWindow>
#include <QTimer>
#include <QEnterEvent>
#include <QEvent>
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(logDock, "dock.window", QtWarningMsg)

static constexpr int DOCK_H = 64;          // Dock 完整高度
static constexpr int HIDDEN_H = 1;         // 隐藏时高度（1px，仍可接收鼠标事件）
static constexpr int HIDE_DELAY_MS = 50;   // 鼠标离开后延迟隐藏的时间

DockWindow::DockWindow(ForeignToplevelManager *manager, DesktopIconResolver *resolver,
                       QScreen *targetScreen, QWidget *parent)
    : QWidget(parent)
    , m_manager(manager)
    , m_resolver(resolver)
    , m_targetScreen(targetScreen)
{
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_NoSystemBackground);
    setWindowFlag(Qt::FramelessWindowHint);
    setMouseTracking(true);

    if (m_targetScreen)
        setScreen(m_targetScreen);

    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 6, 0, 6);
    layout->setSpacing(0);

    layout->addStretch();

    m_taskManager = new TaskManager(m_resolver, this);
    layout->addWidget(m_taskManager);

    m_clock = new ClockWidget(this);
    m_clock->setFixedWidth(90);
    layout->addWidget(m_clock);

    layout->addStretch();

    // 初始隐藏：高度 1px，透明，仍可接收鼠标 enter
    setFixedHeight(HIDDEN_H);

    connect(m_manager, &ForeignToplevelManager::toplevelAdded,
            m_taskManager, &TaskManager::onToplevelAdded);
    connect(m_manager, &ForeignToplevelManager::toplevelRemoved,
            m_taskManager, &TaskManager::onToplevelRemoved);
    connect(m_manager, &ForeignToplevelManager::toplevelChanged,
            m_taskManager, &TaskManager::onToplevelChanged);
    connect(m_taskManager, &TaskManager::activateRequested,
            m_manager, &ForeignToplevelManager::activate);
    // 点击图标激活窗口后立即隐藏 Dock
    connect(m_taskManager, &TaskManager::activateRequested, this, [this](quint32) {
        setHidden(true);
    });

    m_hideTimer = new QTimer(this);
    m_hideTimer->setSingleShot(true);
    m_hideTimer->setInterval(HIDE_DELAY_MS);
    connect(m_hideTimer, &QTimer::timeout, this, [this] { setHidden(true); });

    const QList<quint32> ids = m_manager->toplevelIds();
    qCWarning(logDock) << "pre-populating" << ids.size() << "toplevels for screen"
                       << (m_targetScreen ? m_targetScreen->name() : QStringLiteral("(null)"));
    for (quint32 id : ids)
        m_taskManager->onToplevelAdded(id, m_manager->toplevelInfo(id));
}

DockWindow::~DockWindow() = default;

void DockWindow::setupLayerShell()
{
    m_window = windowHandle();
    if (!m_window) {
        qCWarning(logDock) << "windowHandle() is null";
        return;
    }

    if (m_targetScreen)
        qCWarning(logDock) << "dock on screen" << m_targetScreen->name()
                           << "geometry=" << m_targetScreen->geometry()
                           << "actual QWindow screen="
                           << (m_window->screen() ? m_window->screen()->name() : QStringLiteral("null"));

    using W = LayerShellQt::Window;
    W *lw = W::get(m_window);
    if (!lw) {
        qCWarning(logDock) << "LayerShellQt::Window::get returned null";
        return;
    }
    lw->setLayer(W::LayerTop);
    lw->setAnchors(static_cast<W::Anchor>(W::AnchorLeft | W::AnchorRight | W::AnchorBottom));
    lw->setKeyboardInteractivity(W::KeyboardInteractivityNone);
    lw->setExclusiveZone(-1);
    lw->setScope(QStringLiteral("wlroots-dock"));
    lw->setCloseOnDismissed(true);
    lw->setScreenConfiguration(W::ScreenFromQWindow);
    lw->setMargins(QMargins(0, 0, 0, 0));
}

void DockWindow::setHidden(bool hidden)
{
    if (m_hidden == hidden) return;
    m_hidden = hidden;
    setFixedHeight(hidden ? HIDDEN_H : DOCK_H);
    update();
    qCWarning(logDock) << "setHidden(" << hidden << ") height ->"
                       << (hidden ? HIDDEN_H : DOCK_H);
}

void DockWindow::enterEvent(QEnterEvent *e)
{
    QWidget::enterEvent(e);
    qCWarning(logDock) << "dock Enter!";
    m_hideTimer->stop();
    if (m_hidden)
        setHidden(false);
}

void DockWindow::leaveEvent(QEvent *e)
{
    QWidget::leaveEvent(e);
    qCWarning(logDock) << "dock Leave!";
    // surface 高度变化会触发 labwc 重新配置 layer-surface，产生虚假 leave。
    // 延迟 50ms 确认鼠标是否真的离开：虚假 leave 会被随后的 re-enter 覆盖，
    // 真实 leave 时 underMouse() 仍为 false 才启动隐藏定时器。
    QTimer::singleShot(50, this, [this] {
        if (!underMouse()) {
            m_hideTimer->start();
        } else {
            qCWarning(logDock) << "leave ignored (re-entered)";
        }
    });
}

void DockWindow::paintEvent(QPaintEvent *e)
{
    Q_UNUSED(e);
    if (m_hidden)
        return; // 隐藏状态不绘制，1px 透明
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    QRect content = m_taskManager->geometry().united(m_clock->geometry())
                        .adjusted(-6, -2, 6, 2);
    QPainterPath path;
    path.addRoundedRect(content, 8, 8);
    p.fillPath(path, QColor(20, 20, 24, 200));
}
