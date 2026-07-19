#include "DockController.h"

#include "DockWindow.h"
#include "ForeignToplevelManager.h"
#include "DesktopIconResolver.h"
#include "sni/SniWatcher.h"

#include <QGuiApplication>
#include <QScreen>
#include <QWindow>
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(logCtrl, "dock.ctrl", QtWarningMsg)

DockController::DockController(QObject *parent)
    : QObject(parent) {}

DockController::~DockController()
{
    qDeleteAll(m_docks);
    m_docks.clear();
    delete m_resolver; // 非 QObject，需手动释放
}

bool DockController::init()
{
    m_resolver = new DesktopIconResolver();
    m_manager = new ForeignToplevelManager(this);

    if (!m_manager->connectToDisplay()) {
        qCWarning(logCtrl) << "failed to connect foreign-toplevel manager";
        return false;
    }

    // 初始化 SNI watcher（系统托盘 host）
    m_sni = new SniWatcher(this);
    if (!m_sni->registerHost()) {
        qCWarning(logCtrl) << "failed to register SNI watcher"
                           << "(another tray host may be running)";
        // 不 return：toplevel 管理不受影响，tray 区空白而已
    }

    for (QScreen *s : QGuiApplication::screens())
        addScreen(s);

    connect(qGuiApp, &QGuiApplication::screenAdded,
            this, &DockController::addScreen);
    connect(qGuiApp, &QGuiApplication::screenRemoved,
            this, [this](QScreen *s) { removeScreen(s); });

    return true;
}

void DockController::addScreen(QScreen *screen)
{
    if (!screen) return;
    if (m_docks.contains(screen)) return;
    // 跳过 placeholder screen（切换 TTY 时 Qt 临时创建的空屏幕）
    if (screen->name().isEmpty() || screen->geometry().isEmpty()) {
        qCWarning(logCtrl) << "skipping invalid screen name=" << screen->name()
                           << "geometry=" << screen->geometry();
        return;
    }

    auto w = new DockWindow(m_manager, m_resolver, m_sni, screen);
    // 在 show() 之前强制创建 native window，以便正确设置 screen
    w->winId();
    if (QWindow *win = w->windowHandle()) {
        win->setScreen(screen);
    }
    w->show();
    w->setupLayerShell();
    m_docks.insert(screen, w);
    qCInfo(logCtrl) << "added dock for screen" << screen->name()
                    << "geometry=" << screen->geometry();
}

void DockController::removeScreen(QScreen *screen)
{
    const auto it = m_docks.find(screen);
    if (it == m_docks.end()) return;
    it.value()->deleteLater();
    m_docks.erase(it);
    qCInfo(logCtrl) << "removed dock for screen" << (screen ? screen->name() : QString());
}
