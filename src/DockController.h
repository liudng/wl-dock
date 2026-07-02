#pragma once

#include <QObject>
#include <QHash>

class QScreen;
class ForeignToplevelManager;
class DesktopIconResolver;
class DockWindow;

// 顶层协调者：创建共享的 ForeignToplevelManager 与 DesktopIconResolver，
// 并为每个显示器创建一个 DockWindow（单进程多窗口）。
class DockController : public QObject
{
    Q_OBJECT
public:
    explicit DockController(QObject *parent = nullptr);
    ~DockController() override;

    bool init();

private:
    void addScreen(QScreen *screen);
    void removeScreen(QScreen *screen);

    ForeignToplevelManager *m_manager = nullptr;
    DesktopIconResolver *m_resolver = nullptr;
    QHash<QScreen *, DockWindow *> m_docks;
};
