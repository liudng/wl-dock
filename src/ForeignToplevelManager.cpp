#include "ForeignToplevelManager.h"

#include <QSocketNotifier>
#include <QLoggingCategory>

#include <wayland-client.h>
#include "wlr-foreign-toplevel-management-unstable-v1-client-protocol.h"

Q_LOGGING_CATEGORY(logFtm, "dock.ftm", QtWarningMsg)

ForeignToplevelManager::ForeignToplevelManager(QObject *parent)
    : QObject(parent) {}

ForeignToplevelManager::~ForeignToplevelManager()
{
    if (!m_display)
        return;

    if (m_manager) {
        zwlr_foreign_toplevel_manager_v1_stop(m_manager);
        zwlr_foreign_toplevel_manager_v1_destroy(m_manager);
        m_manager = nullptr;
    }
    if (m_seat) {
        wl_seat_destroy(m_seat);
        m_seat = nullptr;
    }

    // 先释放所有 handle 代理，避免残留 handle 造成资源泄漏
    for (Handle *h : m_byWl) {
        if (h->wlHandle) {
            zwlr_foreign_toplevel_handle_v1_destroy(h->wlHandle);
            h->wlHandle = nullptr;
        }
    }
    qDeleteAll(m_byWl);
    m_byWl.clear();
    m_byId.clear();

    if (m_registry) {
        wl_registry_destroy(m_registry);
        m_registry = nullptr;
    }
    wl_display_disconnect(m_display);
    m_display = nullptr;
}

bool ForeignToplevelManager::connectToDisplay()
{
    m_display = wl_display_connect(nullptr);
    if (!m_display) {
        qCWarning(logFtm) << "wl_display_connect failed";
        return false;
    }

    m_registry = wl_display_get_registry(m_display);
    static const wl_registry_listener regListener = {
        registryGlobal,
        registryGlobalRemove,
    };
    wl_registry_add_listener(m_registry, &regListener, this);

    // 第一次 roundtrip：绑定 manager 与 seat
    wl_display_roundtrip(m_display);

    if (!m_manager) {
        qCWarning(logFtm) << "compositor does not advertise zwlr_foreign_toplevel_management_v1";
        wl_display_disconnect(m_display);
        m_display = nullptr;
        m_registry = nullptr;
        return false;
    }

    // 第二次 roundtrip：接收已存在的 toplevel 的初始事件
    wl_display_roundtrip(m_display);

    qCInfo(logFtm) << "connected; toplevels after roundtrip:" << m_byId.size()
                     << "seat:" << (m_seat ? "yes" : "no");

    const int fd = wl_display_get_fd(m_display);
    m_readNotifier = new QSocketNotifier(fd, QSocketNotifier::Read, this);
    connect(m_readNotifier, &QSocketNotifier::activated,
            this, &ForeignToplevelManager::onSocketActivated);

    flushWl();
    return true;
}

void ForeignToplevelManager::onSocketActivated()
{
    if (wl_display_dispatch(m_display) < 0) {
        qCWarning(logFtm) << "wl_display_dispatch error";
        if (m_readNotifier) m_readNotifier->setEnabled(false);
    }
}

void ForeignToplevelManager::flushWl()
{
    if (m_display) wl_display_flush(m_display);
}

QList<quint32> ForeignToplevelManager::toplevelIds() const
{
    return m_byId.keys();
}

ToplevelInfo ForeignToplevelManager::toplevelInfo(quint32 id) const
{
    auto it = m_byId.constFind(id);
    if (it == m_byId.cend()) return ToplevelInfo();
    return it.value()->info;
}

void ForeignToplevelManager::activate(quint32 id)
{
    auto it = m_byId.find(id);
    if (it == m_byId.end()) return;
    Handle *h = it.value();
    if (h->info.activated) {
        // 已是激活态：按需求保持不变，不发送任何请求
        return;
    }
    if (h->wlHandle && m_seat) {
        zwlr_foreign_toplevel_handle_v1_activate(h->wlHandle, m_seat);
        flushWl();
    } else {
        qCWarning(logFtm) << "activate: missing seat or handle for id" << id;
    }
}

// ---- registry ----
void ForeignToplevelManager::registryGlobal(void *data, wl_registry *registry, uint32_t name,
                                            const char *interface, uint32_t version)
{
    auto self = static_cast<ForeignToplevelManager *>(data);
    if (strcmp(interface, "zwlr_foreign_toplevel_manager_v1") == 0) {
        const uint32_t v = qMin(version, 3u);
        self->m_manager = static_cast<zwlr_foreign_toplevel_manager_v1 *>(
            wl_registry_bind(registry, name, &zwlr_foreign_toplevel_manager_v1_interface, v));
        if (self->m_manager) {
            static const zwlr_foreign_toplevel_manager_v1_listener mgrListener = {
                managerToplevel,
                managerFinished,
            };
            zwlr_foreign_toplevel_manager_v1_add_listener(self->m_manager, &mgrListener, self);
        }
    } else if (strcmp(interface, "wl_seat") == 0 && !self->m_seat) {
        self->m_seat = static_cast<wl_seat *>(
            wl_registry_bind(registry, name, &wl_seat_interface, 1));
    }
}

void ForeignToplevelManager::registryGlobalRemove(void *data, wl_registry *registry, uint32_t name)
{
    Q_UNUSED(data); Q_UNUSED(registry); Q_UNUSED(name)
    // 简化处理：不处理 global 移除
}

// ---- manager ----
void ForeignToplevelManager::managerToplevel(void *data, zwlr_foreign_toplevel_manager_v1 *mgr,
                                             zwlr_foreign_toplevel_handle_v1 *handle)
{
    Q_UNUSED(mgr);
    auto self = static_cast<ForeignToplevelManager *>(data);
    auto h = new Handle();
    h->id = self->m_nextId++;
    h->wlHandle = handle;
    h->manager = self;
    self->m_byWl.insert(handle, h);
    self->m_byId.insert(h->id, h);
    qCWarning(logFtm) << "manager_toplevel: new handle, id=" << h->id;

    static const zwlr_foreign_toplevel_handle_v1_listener listener = {
        handleTitle,
        handleAppId,
        handleOutputEnter,
        handleOutputLeave,
        handleState,
        handleDone,
        handleClosed,
        handleParent,
    };
    zwlr_foreign_toplevel_handle_v1_add_listener(handle, &listener, h);
}

void ForeignToplevelManager::managerFinished(void *data, zwlr_foreign_toplevel_manager_v1 *mgr)
{
    Q_UNUSED(mgr);
    auto self = static_cast<ForeignToplevelManager *>(data);
    qCInfo(logFtm) << "foreign-toplevel manager finished";
    if (self->m_manager) {
        zwlr_foreign_toplevel_manager_v1_destroy(self->m_manager);
        self->m_manager = nullptr;
    }
}

// ---- handle ----
void ForeignToplevelManager::handleTitle(void *data, zwlr_foreign_toplevel_handle_v1 *handle, const char *title)
{
    Q_UNUSED(handle);
    auto h = static_cast<Handle *>(data);
    h->info.title = QString::fromUtf8(title);
}

void ForeignToplevelManager::handleAppId(void *data, zwlr_foreign_toplevel_handle_v1 *handle, const char *app_id)
{
    Q_UNUSED(handle);
    auto h = static_cast<Handle *>(data);
    h->info.appId = QString::fromUtf8(app_id);
}

void ForeignToplevelManager::handleOutputEnter(void *data, zwlr_foreign_toplevel_handle_v1 *handle, wl_output *output)
{
    Q_UNUSED(data); Q_UNUSED(handle); Q_UNUSED(output)
}

void ForeignToplevelManager::handleOutputLeave(void *data, zwlr_foreign_toplevel_handle_v1 *handle, wl_output *output)
{
    Q_UNUSED(data); Q_UNUSED(handle); Q_UNUSED(output)
}

void ForeignToplevelManager::handleState(void *data, zwlr_foreign_toplevel_handle_v1 *handle, wl_array *state)
{
    Q_UNUSED(handle);
    auto h = static_cast<Handle *>(data);
    h->info.maximized = false;
    h->info.minimized = false;
    h->info.activated = false;
    h->info.fullscreen = false;
    // 纯 C++ 迭代 state 数组（避免 wl_array_for_each 宏里的 typeof 扩展）
    const uint32_t *begin = static_cast<const uint32_t *>(state->data);
    const uint32_t *end = begin + state->size / sizeof(uint32_t);
    for (const uint32_t *p = begin; p != end; ++p) {
        switch (*p) {
        case ZWLR_FOREIGN_TOPLEVEL_HANDLE_V1_STATE_MAXIMIZED: h->info.maximized = true; break;
        case ZWLR_FOREIGN_TOPLEVEL_HANDLE_V1_STATE_MINIMIZED: h->info.minimized = true; break;
        case ZWLR_FOREIGN_TOPLEVEL_HANDLE_V1_STATE_ACTIVATED: h->info.activated = true; break;
        case ZWLR_FOREIGN_TOPLEVEL_HANDLE_V1_STATE_FULLSCREEN: h->info.fullscreen = true; break;
        default: break;
        }
    }
}

void ForeignToplevelManager::handleDone(void *data, zwlr_foreign_toplevel_handle_v1 *handle)
{
    Q_UNUSED(handle);
    auto h = static_cast<Handle *>(data);
    auto self = h->manager;
    // 子窗口 app_id 为空时，依次尝试：
    // 1. parent 的 app_id（有 transient for 关系的子窗口）
    // 2. title 作为 app_id（无 parent 的独立子窗口，如微信聊天窗口：
    //    xprop 无 WM_CLASS 也无 WM_TRANSIENT_FOR，但 _NET_WM_NAME="微信"）
    // iconForAppId 会用 title 匹配 .desktop 文件的 Name 字段。
    if (h->info.appId.isEmpty()) {
        if (h->parentHandle) {
            auto parentIt = self->m_byWl.constFind(h->parentHandle);
            if (parentIt != self->m_byWl.cend() && !parentIt.value()->info.appId.isEmpty()) {
                h->info.appId = parentIt.value()->info.appId;
                qCInfo(logFtm) << "toplevel" << h->id << "inherited appId" << h->info.appId
                               << "from parent";
            }
        }
        if (h->info.appId.isEmpty() && !h->info.title.isEmpty()) {
            h->info.appId = h->info.title;
            qCInfo(logFtm) << "toplevel" << h->id << "using title as appId" << h->info.appId;
        }
    }
    if (!h->announced) {
        h->announced = true;
        qCInfo(logFtm) << "toplevel done (first): id=" << h->id
                       << "appId=" << h->info.appId << "title=" << h->info.title
                       << "activated=" << h->info.activated;
        emit self->toplevelAdded(h->id, h->info);
    } else {
        emit self->toplevelChanged(h->id, h->info);
    }
}

void ForeignToplevelManager::handleClosed(void *data, zwlr_foreign_toplevel_handle_v1 *handle)
{
    Q_UNUSED(handle);
    auto h = static_cast<Handle *>(data);
    auto self = h->manager;
    const quint32 id = h->id;
    self->m_byWl.remove(h->wlHandle);
    self->m_byId.remove(id);
    if (h->wlHandle)
        zwlr_foreign_toplevel_handle_v1_destroy(h->wlHandle);
    emit self->toplevelRemoved(id);
    delete h;
}

void ForeignToplevelManager::handleParent(void *data, zwlr_foreign_toplevel_handle_v1 *handle,
                                          zwlr_foreign_toplevel_handle_v1 *parent)
{
    Q_UNUSED(handle);
    auto h = static_cast<Handle *>(data);
    h->parentHandle = parent;
}
