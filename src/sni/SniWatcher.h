#pragma once

#include <QObject>
#include <QHash>
#include <QUuid>

#include "SniTypes.h"

class SniItem;

// StatusNotifierWatcher：在会话总线注册 org.kde.StatusNotifierWatcher 服务，
// 接收各托盘应用的 RegisterStatusNotifierItem 调用，管理 SniItem 生命周期，
// 对外 emit itemAdded / itemRemoved / itemChanged（与 ForeignToplevelManager 同层角色）。
//
// 不依赖 KDE 运行时。协议虽带 org.kde. 前缀，实际是 freedesktop DBus 标准。
class SniWatcher : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.kde.StatusNotifierWatcher")

public:
    explicit SniWatcher(QObject *parent = nullptr);
    ~SniWatcher() override;

    // 注册 watcher 服务 + 对象。返回 false 表示注册失败（可能有其他 host 已占用）。
    bool registerHost();

    bool isRegistered() const { return m_registered; }

    const QHash<QUuid, SniItem *> &items() const { return m_items; }
    QList<QUuid> itemIds() const { return m_items.keys(); }

public slots:
    // org.kde.StatusNotifierWatcher 方法
    void RegisterStatusNotifierItem(const QString &service);

signals:
    // 内部 UI 信号（给 SniTrayWidget 用）
    void itemAdded(const QUuid &id, const TrayItemInfo &info);
    void itemRemoved(const QUuid &id);
    void itemChanged(const QUuid &id, const TrayItemInfo &info);

    // D-Bus 协议信号（ExportAllSignals 会自动导出到总线）
    void StatusNotifierItemRegistered(const QString &service);
    void StatusNotifierItemUnregistered(const QString &service);

private:
    void onItemChanged(SniItem *self);
    void onItemGone(SniItem *self);

    QHash<QUuid, SniItem *> m_items;
    bool m_registered = false;
};
