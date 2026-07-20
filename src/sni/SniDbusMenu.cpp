#include "SniDbusMenu.h"
#include "SniMenu.h"

#include <QDBusInterface>
#include <QDBusArgument>
#include <QDBusMessage>
#include <QDBusConnection>
#include <QDBusObjectPath>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QIcon>
#include <QPoint>
#include <QDateTime>
#include <QWindow>
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(logSniMenu, "dock.sni.menu", QtWarningMsg)

static constexpr auto IFACE_MENU = "com.canonical.dbusmenu";

SniDbusMenu::SniDbusMenu(const QString &service, const QString &path, QObject *parent)
    : QObject(parent), m_service(service), m_path(path)
{
    m_iface = new QDBusInterface(service, path, IFACE_MENU,
                                QDBusConnection::sessionBus(), this);
    m_iface->setTimeout(2000);
}

SniDbusMenu::~SniDbusMenu() = default;

void SniDbusMenu::popup(QWindow *parentWindow, const QPoint &globalPos)
{
    if (!m_iface->isValid()) {
        qCWarning(logSniMenu) << "dbusmenu iface invalid" << m_service << m_path;
        return;
    }

    // 拉取整棵菜单树。GetLayout 签名：
    //   in  int32 parentId (0 = root)
    //   in  int32 recursionDepth (-1 = infinite)
    //   in  array<string> propertyNames (empty = all)
    //   out uint32 revision
    //   out variant layout  -- 实际是 (ia{sv}av) 结构
    QDBusMessage msg = QDBusMessage::createMethodCall(
        m_service, m_path, IFACE_MENU, QStringLiteral("GetLayout"));
    msg << 0 << -1 << QStringList();

    QDBusMessage reply = QDBusConnection::sessionBus().call(msg, QDBus::Block, 2000);
    if (reply.type() != QDBusMessage::ReplyMessage || reply.arguments().size() < 2) {
        qCWarning(logSniMenu) << "GetLayout failed" << m_service << reply.errorMessage();
        return;
    }

    // 第二个返回值签名按规范是 variant(包裹 (ia{sv}av))，但部分实现（含 fcitx5）
    // 直接返回 struct（QDBusArgument）。两种都要兼容。
    const QVariant secondArg = reply.arguments().at(1);
    QDBusArgument layoutArg;
    if (secondArg.userType() == qMetaTypeId<QDBusVariant>()) {
        const QVariant inner = secondArg.value<QDBusVariant>().variant();
        if (inner.userType() == qMetaTypeId<QDBusArgument>())
            layoutArg = inner.value<QDBusArgument>();
    } else if (secondArg.userType() == qMetaTypeId<QDBusArgument>()) {
        layoutArg = secondArg.value<QDBusArgument>();
    }

    if (layoutArg.currentType() != QDBusArgument::StructureType) {
        qCWarning(logSniMenu) << "GetLayout: unexpected layout type, secondArg typeName="
                              << secondArg.typeName() << "userType=" << secondArg.userType();
        return;
    }

    const MenuItem root = parseMenuItem(layoutArg);

    QList<SniMenu::Item> items;
    buildItems(root.children, items);
    if (items.isEmpty()) {
        qCWarning(logSniMenu) << "GetLayout returned empty menu" << m_service;
        return;
    }

    // 提前发送 AboutToShow(0) 通知（部分应用靠这个信号初始化菜单内容）
    QDBusMessage aboutMsg = QDBusMessage::createMethodCall(
        m_service, m_path, IFACE_MENU, QStringLiteral("AboutToShow"));
    aboutMsg << 0;
    QDBusConnection::sessionBus().asyncCall(aboutMsg);

    // 自绘菜单（避开 LayerShellQt popup attach 失败 + 屏幕中央定位问题）
    SniMenu menu;
    menu.setItems(items);
    const int clickedId = menu.popup(parentWindow, globalPos);
    if (clickedId >= 0)
        onActionTriggered(clickedId);
}

SniDbusMenu::MenuItem SniDbusMenu::parseMenuItem(const QDBusArgument &arg)
{
    // (ia{sv}av)：
    //   int id
    //   a{sv} properties
    //   av children  -- 每个 variant 内嵌 (ia{sv}av) 递归结构
    MenuItem item;
    arg.beginStructure();
    arg >> item.id;
    arg >> item.properties;
    arg.beginArray();
    while (!arg.atEnd()) {
        QVariant childVar;
        arg >> childVar;
        if (childVar.canConvert<QDBusArgument>()) {
            const QDBusArgument childArg = childVar.value<QDBusArgument>();
            if (childArg.currentType() == QDBusArgument::StructureType)
                item.children.append(parseMenuItem(childArg));
        }
    }
    arg.endArray();
    arg.endStructure();
    return item;
}

void SniDbusMenu::buildItems(const QList<MenuItem> &items, QList<SniMenu::Item> &out)
{
    for (const MenuItem &item : items) {
        const QString type = item.properties.value(QStringLiteral("type")).toString();
        const bool visible = item.properties.value(QStringLiteral("visible"), true).toBool();
        const bool enabled = item.properties.value(QStringLiteral("enabled"), true).toBool();
        if (!visible)
            continue;

        if (type == QLatin1String("separator")) {
            SniMenu::Item m;
            m.separator = true;
            out.append(m);
            continue;
        }

        // DBusMenu 用 '_' 标记快捷键前缀。SniMenu 不用快捷键字符，
        // 直接去掉 '_' 让显示更干净（"__" 这种字面量场景极少见）。
        QString label = item.properties.value(QStringLiteral("label")).toString();
        label.remove(QChar('_'));

        const QString iconName = item.properties.value(QStringLiteral("icon-name")).toString();
        QIcon icon;
        if (!iconName.isEmpty())
            icon = QIcon::fromTheme(iconName);

        SniMenu::Item m;
        m.label = label;
        m.icon = icon;
        m.enabled = enabled;
        m.id = item.id;

        const QString toggleType = item.properties.value(QStringLiteral("toggle-type")).toString();
        const int toggleState = item.properties.value(QStringLiteral("toggle-state"), -1).toInt();
        if (toggleType == QLatin1String("checkmark") || toggleType == QLatin1String("radio")) {
            m.checkable = true;
            m.checked = (toggleState == 1);
        }

        // 子菜单暂不展开（fcitx5 等主菜单都不用）
        out.append(m);
    }
}

void SniDbusMenu::onActionTriggered(int id)
{
    // DBusMenu 协议要求点击前先发 AboutToShow(id)，部分实现（含 fcitx5）
    // 没收到 AboutToShow 会忽略 clicked 事件。
    QDBusMessage aboutMsg = QDBusMessage::createMethodCall(
        m_service, m_path, IFACE_MENU, QStringLiteral("AboutToShow"));
    aboutMsg << id;
    QDBusConnection::sessionBus().call(aboutMsg, QDBus::Block, 1000);

    // 发送 Event(id, "clicked", data, timestamp)
    // data 是 variant，QtDBus 不接受 null QVariant，传一个空 int 包装的 variant
    QDBusMessage msg = QDBusMessage::createMethodCall(
        m_service, m_path, IFACE_MENU, QStringLiteral("Event"));
    msg << id << QStringLiteral("clicked")
        << QVariant::fromValue(QDBusVariant(QVariant::fromValue(0)))
        << static_cast<quint32>(QDateTime::currentMSecsSinceEpoch() & 0xffffffffu);

    QDBusPendingCall call = QDBusConnection::sessionBus().asyncCall(msg);
    auto *watcher = new QDBusPendingCallWatcher(call, this);
    QObject::connect(watcher, &QDBusPendingCallWatcher::finished, this, [watcher]() {
        QDBusPendingReply<> reply = *watcher;
        if (reply.isError())
            qCWarning(logSniMenu) << "Event call failed:" << reply.error().message();
        else
            qCDebug(logSniMenu) << "Event call ok";
        watcher->deleteLater();
    });
}
