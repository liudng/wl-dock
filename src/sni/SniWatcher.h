#pragma once

#include <QObject>
#include <QHash>
#include <QUuid>

#include "SniTypes.h"

class SniItem;

// 内部 UI 信号集合：从 SniWatcher 分离出来，避免 SniWatcher 上的
// Q_CLASSINFO("D-Bus Interface", ...) 触发 Qt 自动挂载的 QDBusAdaptorConnector
// 监听 SniWatcher 所有信号并尝试 marshal 到 D-Bus —— 那会导致参数列表里
// 未注册到 QtDBus 的 QUuid 触发 "Cannot relay signal" 警告。
//
// 该对象本身从不通过 registerObject 暴露到总线，所以可以自由 emit 任意类型。
class SniWatcherSignals : public QObject
{
    Q_OBJECT
public:
    explicit SniWatcherSignals(QObject *parent = nullptr) : QObject(parent) {}

signals:
    void itemAdded(const QUuid &id, const TrayItemInfo &info);
    void itemRemoved(const QUuid &id);
    void itemChanged(const QUuid &id, const TrayItemInfo &info);
};

// StatusNotifierWatcher：在会话总线注册 org.kde.StatusNotifierWatcher 服务，
// 接收各托盘应用的 RegisterStatusNotifierItem 调用，管理 SniItem 生命周期。
// UI 层通过 ui() 拿到的 SniWatcherSignals 监听 item 增删改（不导出到 D-Bus）。
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

    // UI 信号访问器（itemAdded / itemRemoved / itemChanged）
    SniWatcherSignals *ui() const { return m_ui; }

public slots:
    // org.kde.StatusNotifierWatcher 方法（导出到 D-Bus）
    Q_SCRIPTABLE void RegisterStatusNotifierItem(const QString &service);

signals:
    // D-Bus 协议信号（导出到总线，按 SNI 规范通知其他监听者）
    Q_SCRIPTABLE void StatusNotifierItemRegistered(const QString &service);
    Q_SCRIPTABLE void StatusNotifierItemUnregistered(const QString &service);

private:
    void onItemChanged(SniItem *self);
    void onItemGone(SniItem *self);

    QHash<QUuid, SniItem *> m_items;
    bool m_registered = false;
    SniWatcherSignals *m_ui;
};
