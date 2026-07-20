#pragma once

#include <QObject>
#include <QHash>

class QScreen;
class ForeignToplevelManager;
class DesktopIconResolver;
class DockWindow;
class SniWatcher;

// 顶层协调者：创建共享的 ForeignToplevelManager 与 DesktopIconResolver，
// 并为每个显示器创建一个 DockWindow（单进程多窗口）。
class DockController : public QObject
{
    Q_OBJECT
public:
    explicit DockController(QObject *parent = nullptr);
    ~DockController() override;

    // defaultIconName: 查不到 .desktop 时的兜底图标名称（对应 QIcon::fromTheme 名），
    // 空则用 DesktopIconResolver 内部默认值 "application-x-executable"。
    bool init(const QString &defaultIconName = QString());

    SniWatcher *sniWatcher() const { return m_sni; }

private:
    void addScreen(QScreen *screen);
    void removeScreen(QScreen *screen);

    ForeignToplevelManager *m_manager = nullptr;
    DesktopIconResolver *m_resolver = nullptr;
    SniWatcher *m_sni = nullptr;
    QHash<QScreen *, DockWindow *> m_docks;
};
