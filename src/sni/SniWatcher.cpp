#include "SniWatcher.h"
#include "SniItem.h"

#include <QDBusConnection>
#include <QDBusConnectionInterface>


static constexpr auto BUS_WATCHER = "org.kde.StatusNotifierWatcher";
static constexpr auto PATH_WATCHER = "/StatusNotifierWatcher";
static constexpr auto IFACE_WATCHER = "org.kde.StatusNotifierWatcher";

SniWatcher::SniWatcher(QObject *parent)
    : QObject(parent)
    , m_ui(new SniWatcherSignals(this))
{
}

SniWatcher::~SniWatcher()
{
    for (auto it = m_items.begin(); it != m_items.end(); ++it)
        delete it.value();
    m_items.clear();
    if (m_registered) {
        QDBusConnection::sessionBus().unregisterObject(PATH_WATCHER);
        QDBusConnection::sessionBus().unregisterService(BUS_WATCHER);
    }
}

bool SniWatcher::registerHost()
{
    auto bus = QDBusConnection::sessionBus();

    if (!bus.registerService(BUS_WATCHER)) {
        qCWarning(logSni) << "Failed to register" << BUS_WATCHER
                           << "— another tray host may already be running."
                           << bus.lastError().message();
        return false;
    }

    // 只导出 Q_SCRIPTABLE 标注的 slot/signal：
    //   - RegisterStatusNotifierItem（方法）
    //   - StatusNotifierItemRegistered / Unregistered（协议信号）
    // 内部 UI 信号 itemAdded/itemRemoved/itemChanged 含 QUuid/TrayItemInfo，
    // 未在 QtDBus 注册且不该外露，不能跟着 ExportAllSignals 一起出去。
    if (!bus.registerObject(PATH_WATCHER, IFACE_WATCHER, this,
                             QDBusConnection::ExportScriptableSlots |
                             QDBusConnection::ExportScriptableSignals |
                             QDBusConnection::ExportAllProperties)) {
        qCWarning(logSni) << "Failed to register watcher object:" << bus.lastError().message();
        bus.unregisterService(BUS_WATCHER);
        return false;
    }

    m_registered = true;
    qCInfo(logSni) << "StatusNotifierWatcher registered successfully";
    return true;
}

void SniWatcher::RegisterStatusNotifierItem(const QString &service)
{
    // service 参数可能是：
    //   "org.kde.StatusNotifierItem-PID-N"   (well-known name, path 默认 /StatusNotifierItem)
    //   ":1.42"                                (unique name, path 默认 /StatusNotifierItem)
    //   "org.kde.StatusNotifierItem-PID-N/StatusNotifierItem"  (name+path)
    //   ":1.42/CustomPath"                     (unique name+path)
    // 按**最后一个** '/' 切分 name 和 path
    QString name, path;
    const int slash = service.lastIndexOf(QLatin1Char('/'));
    if (slash >= 0) {
        name  = service.left(slash);
        path  = service.mid(slash + 1);
    } else {
        name  = service;
        path  = QStringLiteral("/StatusNotifierItem");
    }

    if (name.isEmpty() || path.isEmpty()) {
        qCWarning(logSni) << "Invalid RegisterStatusNotifierItem arg:" << service;
        return;
    }

    // 去重：同 service+path 不重复注册
    for (auto it = m_items.constBegin(); it != m_items.constEnd(); ++it) {
        if (it.value()->service() == name && it.value()->path() == path) {
            qCWarning(logSni) << "Duplicate SNI registration ignored:" << service;
            return;
        }
    }

    qCInfo(logSni) << "SNI item registered:" << name << path;

    auto *item = new SniItem(name, path, this);
    const QUuid id = QUuid::createUuid();
    m_items.insert(id, item);

    connect(item, &SniItem::changed, this, [this, id](SniItem *self) { onItemChanged(self); });
    connect(item, &SniItem::gone,    this, [this, id](SniItem *self) { onItemGone(self); });

    if (!item->init()) {
        // 远端不可达（应用可能已退出），清理
        m_items.remove(id);
        delete item;
        return;
    }

    emit m_ui->itemAdded(id, item->snapshot());
    emit StatusNotifierItemRegistered(service);
}

void SniWatcher::onItemChanged(SniItem *self)
{
    // 找到 id
    for (auto it = m_items.constBegin(); it != m_items.constEnd(); ++it) {
        if (it.value() == self) {
            emit m_ui->itemChanged(it.key(), self->snapshot());
            return;
        }
    }
}

void SniWatcher::onItemGone(SniItem *self)
{
    for (auto it = m_items.constBegin(); it != m_items.constEnd(); ++it) {
        if (it.value() == self) {
            const QUuid id = it.key();
            const QString svc = self->service() + self->path();
            m_items.erase(it);
            delete self;
            emit m_ui->itemRemoved(id);
            emit StatusNotifierItemUnregistered(svc);
            return;
        }
    }
}
