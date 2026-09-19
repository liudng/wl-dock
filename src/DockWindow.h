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
class QPointF;

namespace LayerShellQt { class Window; }

// 单个 Dock 窗口（对应一个显示器）。包含左侧任务管理器、右侧系统托盘与时钟，
// 通过 LayerShellQt 贴底居中。自动隐藏实现：窗口始终贴底，隐藏时
// 高度缩为 1px（透明、可接收鼠标事件），鼠标贴靠屏幕底边且处于
// dock 内容宽度范围内时恢复完整高度；鼠标离开窗口或移出内容宽度
// 范围（两侧空白区域）后延迟缩回 1px。
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
    void mouseMoveEvent(QMouseEvent *e) override;

private:
    void setHidden(bool hidden);
    // 鼠标 x 坐标（窗口坐标系）是否位于 dock 内容宽度范围内
    bool withinContentX(qreal x) const;
    // 显示状态下鼠标是否位于可见内容条区域内：水平在内容宽度范围内，
    // 且 y 不低于内容条顶部（上方 TIP_RESERVE 是为 tooltip 预留的
    // 透明区域，视为 dock 外部）
    bool overContentBar(const QPointF &pos) const;

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
