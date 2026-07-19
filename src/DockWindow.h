#pragma once

#include <QWidget>

class TaskManager;
class ClockWidget;
class ForeignToplevelManager;
class DesktopIconResolver;
class SniWatcher;
class SniTrayWidget;
class QWindow;
class QScreen;
class QEnterEvent;
class QTimer;

namespace LayerShellQt { class Window; }

// 单个 Dock 窗口（对应一个显示器）。包含左侧任务管理器、右侧系统托盘与时钟，
// 通过 LayerShellQt 贴底居中。自动隐藏实现：窗口始终贴底，隐藏时
// 高度缩为 1px（透明、可接收鼠标事件），鼠标进入后恢复完整高度，
// 鼠标离开后延迟缩回 1px。
class DockWindow : public QWidget
{
    Q_OBJECT
public:
    DockWindow(ForeignToplevelManager *manager, DesktopIconResolver *resolver,
               SniWatcher *sni, QScreen *targetScreen = nullptr, QWidget *parent = nullptr);
    ~DockWindow();

    TaskManager *taskManager() const { return m_taskManager; }
    ForeignToplevelManager *manager() const { return m_manager; }

    // 在窗口 show() 之后调用，配置 layer shell 属性
    void setupLayerShell();

protected:
    void paintEvent(QPaintEvent *e) override;
    void enterEvent(QEnterEvent *e) override;
    void leaveEvent(QEvent *e) override;

private:
    void setHidden(bool hidden);

    ForeignToplevelManager *m_manager;
    DesktopIconResolver *m_resolver;
    SniWatcher *m_sni;
    TaskManager *m_taskManager = nullptr;
    SniTrayWidget *m_tray = nullptr;
    ClockWidget *m_clock = nullptr;
    QWindow *m_window = nullptr;
    QScreen *m_targetScreen = nullptr;
    bool m_hidden = true;
    QTimer *m_hideTimer = nullptr;
};
