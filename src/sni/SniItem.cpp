#include "SniItem.h"
#include "SniDbusMenu.h"

#include <QDBusInterface>
#include <QDBusArgument>
#include <QDBusObjectPath>
#include <QDBusPendingReply>
#include <QDBusConnection>
#include <QDBusServiceWatcher>
#include <QImage>
#include <QBuffer>
#include <QPixmap>
#include <QPoint>
#include <QWidget>
#include <QWindow>
#include <QRegularExpression>
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(logSni, "dock.sni", QtWarningMsg)

static constexpr auto IFACE_ITEM = "org.kde.StatusNotifierItem";

SniItem::SniItem(const QString &service, const QString &path, QObject *parent)
    : QObject(parent)
    , m_service(service)
    , m_path(path)
{
    m_iface = new QDBusInterface(service, path, IFACE_ITEM,
                                QDBusConnection::sessionBus(), this);
    m_iface->setTimeout(2000);

    // 监视远端 service：从总线消失时通知 watcher 清理
    auto *watcher = new QDBusServiceWatcher(service, QDBusConnection::sessionBus(),
                                            QDBusServiceWatcher::WatchForOwnerChange, this);
    connect(watcher, &QDBusServiceWatcher::serviceOwnerChanged,
            this, &SniItem::onServiceOwnerChanged);
}

SniItem::~SniItem() = default;

bool SniItem::init()
{
    if (!m_iface->isValid()) {
        qCWarning(logSni) << "SNI item iface invalid" << m_service << m_path;
        return false;
    }
    refreshAll();

    // 订阅属性变更信号（legacy 接口，无参数，到达后重读对应属性）
    QDBusConnection::sessionBus().connect(
        m_service, m_path, IFACE_ITEM, QStringLiteral("NewIcon"), QString(),
        this, SLOT(onNewIcon()));
    QDBusConnection::sessionBus().connect(
        m_service, m_path, IFACE_ITEM, QStringLiteral("NewTitle"), QString(),
        this, SLOT(onNewTitle()));
    QDBusConnection::sessionBus().connect(
        m_service, m_path, IFACE_ITEM, QStringLiteral("NewStatus"), QString(),
        this, SLOT(onNewStatus()));
    QDBusConnection::sessionBus().connect(
        m_service, m_path, IFACE_ITEM, QStringLiteral("NewToolTip"), QString(),
        this, SLOT(onNewToolTip()));
    return true;
}

void SniItem::refreshAll()
{
    // 通过 org.freedesktop.DBus.Properties.GetAll 一次拿到所有属性
    QDBusMessage msg = QDBusMessage::createMethodCall(
        m_service, m_path, QStringLiteral("org.freedesktop.DBus.Properties"),
        QStringLiteral("GetAll"));
    msg << QString(IFACE_ITEM);
    QDBusMessage reply = QDBusConnection::sessionBus().call(msg, QDBus::Block, 2000);
    if (reply.type() != QDBusMessage::ReplyMessage) {
        qCWarning(logSni) << "SNI GetAll failed" << m_service << reply.errorMessage();
        return;
    }
    const QVariant arg = reply.arguments().isEmpty() ? QVariant() : reply.arguments().at(0);
    const QDBusArgument dbusArg = arg.value<QDBusArgument>();
    if (dbusArg.currentType() != QDBusArgument::MapType) {
        qCWarning(logSni) << "SNI GetAll unexpected type";
        return;
    }

    QVariantMap props;
    dbusArg >> props;

    m_id       = props.value(QStringLiteral("Id")).toString();
    m_title    = props.value(QStringLiteral("Title")).toString();
    m_status   = props.value(QStringLiteral("Status")).toString();
    m_iconName = props.value(QStringLiteral("IconName")).toString();
    m_iconPixmap = decodePixmapArray(props.value(QStringLiteral("IconPixmap")));
    m_attentionIconName = props.value(QStringLiteral("AttentionIconName")).toString();
    m_attentionIconPixmap = decodePixmapArray(props.value(QStringLiteral("AttentionIconPixmap")));
    m_tooltipText = toolTipToString(props.value(QStringLiteral("ToolTip")));
    m_itemIsMenu = props.value(QStringLiteral("ItemIsMenu")).toBool();

    // Menu 属性：QDBusObjectPath，指向 com.canonical.dbusmenu 对象。
    // 非空时 host 主动渲染菜单（覆盖大多数现代 SNI 应用，包括 fcitx5）。
    const QVariant menuVar = props.value(QStringLiteral("Menu"));
    if (menuVar.isValid()) {
        QString menuPath;
        if (menuVar.userType() == qMetaTypeId<QDBusObjectPath>())
            menuPath = menuVar.value<QDBusObjectPath>().path();
        else
            menuPath = menuVar.toString();
        if (!menuPath.isEmpty() && menuPath != QLatin1String("/")) {
            m_menuPath = menuPath;
            if (!m_menu)
                m_menu = new SniDbusMenu(m_service, m_menuPath, this);
        }
    }
}

TrayItemInfo SniItem::snapshot() const
{
    return TrayItemInfo{
        .id = m_id.isEmpty() ? m_service : m_id,
        .title = m_title,
        .status = m_status,
        .icon = buildIcon(),
        .tooltipText = m_tooltipText,
        .itemIsMenu = m_itemIsMenu,
    };
}

void SniItem::activate(int x, int y)
{
    m_iface->asyncCall(QStringLiteral("Activate"), x, y);
}

void SniItem::secondaryActivate(int x, int y)
{
    m_iface->asyncCall(QStringLiteral("SecondaryActivate"), x, y);
}

void SniItem::contextMenu(QWidget *parentWidget, const QPoint &globalPos)
{
    // 优先路径 B：host 主动渲染 DBusMenu（fcitx5 / KDE / Telegram 等都走这条）
    if (m_menu) {
        // parentWidget 是 SniTrayWidget 等子 widget，windowHandle() 为 nullptr。
        // 用 window()->windowHandle() 取顶层 DockWindow 的 QWindow，
        // SniMenu 需要它来获知 dock 所在屏幕（多屏定位）。
        QWindow *dockWindow = parentWidget ? parentWidget->window()->windowHandle() : nullptr;
        m_menu->popup(dockWindow, globalPos);
        return;
    }
    // 回退路径 A：让远端自己弹菜单。Wayland 下传 (0,0)，
    // 远端用 QCursor::pos() / gdk_device_get_position 自取鼠标位置。
    m_iface->asyncCall(QStringLiteral("ContextMenu"), 0, 0);
}

void SniItem::onNewIcon()
{
    // NewIcon 只通知图标变了，重读 IconName + IconPixmap
    readPropertyAsync("IconName", [this](const QVariant &v) {
        m_iconName = v.toString();
    });
    readPropertyAsync("IconPixmap", [this](const QVariant &v) {
        m_iconPixmap = decodePixmapArray(v);
        emit changed(this);
    });
}

void SniItem::onNewTitle()
{
    readPropertyAsync("Title", [this](const QVariant &v) {
        m_title = v.toString();
        emit changed(this);
    });
}

void SniItem::onNewStatus()
{
    readPropertyAsync("Status", [this](const QVariant &v) {
        m_status = v.toString();
        emit changed(this);
    });
}

void SniItem::onNewToolTip()
{
    readPropertyAsync("ToolTip", [this](const QVariant &v) {
        m_tooltipText = toolTipToString(v);
        emit changed(this);
    });
}

void SniItem::onServiceOwnerChanged(const QString &name, const QString &oldOwner, const QString &newOwner)
{
    Q_UNUSED(name);
    if (!newOwner.isEmpty())
        return; // 只关心 service 消失
    Q_UNUSED(oldOwner);
    emit gone(this);
}

void SniItem::readPropertyAsync(const char *propName,
                                std::function<void(const QVariant&)> cb)
{
    // 通过 org.freedesktop.DBus.Properties.Get 异步读单属性
    QDBusMessage msg = QDBusMessage::createMethodCall(
        m_service, m_path, QStringLiteral("org.freedesktop.DBus.Properties"),
        QStringLiteral("Get"));
    msg << QString(IFACE_ITEM) << QString::fromLatin1(propName);
    auto *call = new QDBusPendingCallWatcher(QDBusConnection::sessionBus().asyncCall(msg, 2000), this);
    QObject::connect(call, &QDBusPendingCallWatcher::finished, this,
                     [call, cb](QDBusPendingCallWatcher *) {
                         QDBusPendingReply<QDBusVariant> reply = *call;
                         if (reply.isError()) {
                             qCWarning(logSni) << "SNI Get failed:" << reply.error().message();
                         } else {
                             cb(reply.value().variant());
                         }
                         call->deleteLater();
                     });
}

QIcon SniItem::buildIcon() const
{
    // NeedsAttention 状态优先用 attention 图标
    if (m_status == QLatin1String("NeedsAttention")) {
        if (!m_attentionIconName.isEmpty()) {
            QIcon ic = fromIconName(m_attentionIconName);
            if (!ic.isNull()) return ic;
        }
        if (!m_attentionIconPixmap.isEmpty())
            return fromPixmapArray(m_attentionIconPixmap);
    }
    // 正常态：优先 IconName，回退 IconPixmap
    if (!m_iconName.isEmpty()) {
        QIcon ic = fromIconName(m_iconName);
        if (!ic.isNull()) return ic;
    }
    if (!m_iconPixmap.isEmpty())
        return fromPixmapArray(m_iconPixmap);
    return QIcon();
}

QIcon SniItem::fromIconName(const QString &name)
{
    return QIcon::fromTheme(name);
}

QIcon SniItem::fromPixmapArray(const QList<PixmapEntry> &pixmaps)
{
    QIcon ic;
    if (pixmaps.isEmpty())
        return ic;
    // 数组通常按尺寸降序；为每个尺寸都加一个 mode，便于按目标尺寸缩放
    for (const PixmapEntry &e : pixmaps) {
        if (e.width <= 0 || e.height <= 0 || e.bytes.size() < e.width * e.height * 4)
            continue;
        // SNI 字节序：大端 ARGB32（内存顺序 A,R,G,B）
        QImage img(e.width, e.height, QImage::Format_ARGB32);
        const int pixelCount = e.width * e.height;
        // copy 出来（dbus 数据只读），并按 32-bit 字反转字节序（大端→小端）
        const uchar *src = reinterpret_cast<const uchar *>(e.bytes.constData());
        uchar *dst = img.bits();
        for (int i = 0; i < pixelCount; ++i) {
            dst[4 * i + 0] = src[4 * i + 3]; // B <- B(原位置 3)
            dst[4 * i + 1] = src[4 * i + 2]; // G
            dst[4 * i + 2] = src[4 * i + 1]; // R
            dst[4 * i + 3] = src[4 * i + 0]; // A <- A(原位置 0)
        }
        ic.addPixmap(QPixmap::fromImage(std::move(img)));
    }
    return ic;
}

QList<SniItem::PixmapEntry> SniItem::decodePixmapArray(const QVariant &v)
{
    QList<PixmapEntry> result;
    if (!v.isValid())
        return result;
    const QDBusArgument arg = v.value<QDBusArgument>();
    if (arg.currentType() != QDBusArgument::ArrayType)
        return result;
    // a(iiay)：数组，每元素是结构体 (int width, int height, byte[] bytes)
    arg.beginArray();
    while (!arg.atEnd()) {
        arg.beginStructure();
        int w = 0, h = 0;
        QByteArray bytes;
        arg >> w >> h >> bytes;
        arg.endStructure();
        result.append(PixmapEntry{.width = w, .height = h, .bytes = bytes});
    }
    arg.endArray();
    return result;
}

QString SniItem::toolTipToString(const QVariant &v)
{
    // ToolTip 签名 (sa(iiay)ss)：icon-name, icon-pixmap-array, title, subtitle
    if (!v.isValid())
        return {};
    const QDBusArgument arg = v.value<QDBusArgument>();
    if (arg.currentType() != QDBusArgument::StructureType)
        return {};
    arg.beginStructure();
    QString iconName;
    arg >> iconName;
    // 跳过中间的 a(iiay) 图标数据
    arg.beginArray();
    while (!arg.atEnd()) {
        arg.beginStructure();
        int w = 0, h = 0; QByteArray b;
        arg >> w >> h >> b;
        arg.endStructure();
    }
    arg.endArray();
    QString title;
    QString subtitle;
    arg >> title >> subtitle;
    arg.endStructure();

    // 剥去富文本标签，简化为一行纯文本
    auto strip = [](const QString &s) {
        QString out = s;
        out.remove(QStringLiteral("<br>"), Qt::CaseInsensitive);
        out.remove(QRegularExpression(QStringLiteral("<[^>]*>")));
        return out.trimmed();
    };
    const QString t = strip(title);
    const QString sub = strip(subtitle);
    if (t.isEmpty()) return sub;
    if (sub.isEmpty()) return t;
    return t + QStringLiteral(" — ") + sub;
}
