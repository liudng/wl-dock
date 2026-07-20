#pragma once

#include <QObject>
#include <QString>
#include <QList>
#include <QVariantMap>

#include "SniMenu.h"

class QDBusInterface;
class QDBusArgument;
class QPoint;
class QWindow;

// com.canonical.dbusmenu 客户端：从远端拉取菜单结构，用自绘 SniMenu 渲染。
//
// 用于支持那些不实现 SNI ContextMenu 方法、只暴露 Menu 属性的托盘应用
// （典型：fcitx5、KDE 系列、Telegram 等）。SNI 规范允许两条路径，
// 这是路径 B 的最小实现。
//
// 限制（简化版）：
//   - 每次右键都重新 GetLayout 拉整棵菜单（< 50ms，可接受）
//   - 不订阅 ItemUpdated/ItemPropertyUpdated 实时信号（菜单只在打开瞬间是最新）
//   - 不处理快捷键、动态图标
//   - 不支持子菜单（fcitx5 等主菜单都不用，需要时再扩展）
class SniDbusMenu : public QObject
{
    Q_OBJECT
public:
    SniDbusMenu(const QString &service, const QString &path, QObject *parent = nullptr);
    ~SniDbusMenu() override;

    // 在 globalPos 处弹出菜单。同步调用 GetLayout（阻塞 < 2s 超时）。
    // parentWindow 当前未使用（SniMenu 通过 LayerShellQt margin 自定位），
    // 保留参数以兼容调用方。
    void popup(QWindow *parentWindow, const QPoint &globalPos);

    // DBusMenu 节点结构（GetLayout 返回的递归树）
    struct MenuItem {
        int id = 0;
        QVariantMap properties;
        QList<MenuItem> children;
    };

private:
    void buildItems(const QList<MenuItem> &items, QList<SniMenu::Item> &out);
    void onActionTriggered(int id);
    static MenuItem parseMenuItem(const QDBusArgument &arg);

    QString m_service;
    QString m_path;
    QDBusInterface *m_iface = nullptr;
};
