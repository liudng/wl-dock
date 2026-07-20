#pragma once

#include <QObject>
#include <QHash>
#include <QList>
#include <QString>
#include <QMetaType>

struct wl_display;
struct wl_registry;
struct wl_seat;
struct wl_array;
struct wl_output;
struct zwlr_foreign_toplevel_manager_v1;
struct zwlr_foreign_toplevel_handle_v1;

// 单个 toplevel 窗口的快照信息
struct ToplevelInfo {
    QString title;
    QString appId;
    bool maximized = false;
    bool minimized = false;
    bool activated = false;
    bool fullscreen = false;
};
Q_DECLARE_METATYPE(ToplevelInfo)

// 通过独立的 Wayland 连接绑定 wlr-foreign-toplevel-management-v1 协议，
// 用 QSocketNotifier 集成到 Qt 事件循环中。
class ForeignToplevelManager : public QObject
{
    Q_OBJECT
public:
    explicit ForeignToplevelManager(QObject *parent = nullptr);
    ~ForeignToplevelManager() override;

    bool connectToDisplay();
    bool isConnected() const { return m_display != nullptr; }

    QList<quint32> toplevelIds() const;
    ToplevelInfo toplevelInfo(quint32 id) const;

    // 激活指定窗口；若该窗口已是激活态则不做事（按需求约定）
    void activate(quint32 id);

signals:
    void toplevelAdded(quint32 id, const ToplevelInfo &info);
    void toplevelRemoved(quint32 id);
    void toplevelChanged(quint32 id, const ToplevelInfo &info);

private:
    struct Handle {
        quint32 id = 0;
        zwlr_foreign_toplevel_handle_v1 *wlHandle = nullptr;
        ForeignToplevelManager *manager = nullptr;
        ToplevelInfo info;
        bool announced = false;
        // wlr-foreign-toplevel-management 的 parent 事件记录的父 toplevel。
        // 子窗口（如微信聊天窗口）app_id 可能为空，此时用 parent 的 app_id
        // 作为图标识别的 fallback。
        zwlr_foreign_toplevel_handle_v1 *parentHandle = nullptr;
    };

    // registry
    static void registryGlobal(void *data, wl_registry *registry, uint32_t name,
                               const char *interface, uint32_t version);
    static void registryGlobalRemove(void *data, wl_registry *registry, uint32_t name);

    // manager
    static void managerToplevel(void *data, zwlr_foreign_toplevel_manager_v1 *mgr,
                                zwlr_foreign_toplevel_handle_v1 *handle);
    static void managerFinished(void *data, zwlr_foreign_toplevel_manager_v1 *mgr);

    // handle
    static void handleTitle(void *data, zwlr_foreign_toplevel_handle_v1 *handle, const char *title);
    static void handleAppId(void *data, zwlr_foreign_toplevel_handle_v1 *handle, const char *app_id);
    static void handleOutputEnter(void *data, zwlr_foreign_toplevel_handle_v1 *handle, wl_output *output);
    static void handleOutputLeave(void *data, zwlr_foreign_toplevel_handle_v1 *handle, wl_output *output);
    static void handleState(void *data, zwlr_foreign_toplevel_handle_v1 *handle, wl_array *state);
    static void handleDone(void *data, zwlr_foreign_toplevel_handle_v1 *handle);
    static void handleClosed(void *data, zwlr_foreign_toplevel_handle_v1 *handle);
    static void handleParent(void *data, zwlr_foreign_toplevel_handle_v1 *handle,
                             zwlr_foreign_toplevel_handle_v1 *parent);

    void onSocketActivated();
    void flushWl();

    wl_display *m_display = nullptr;
    wl_registry *m_registry = nullptr;
    class QSocketNotifier *m_readNotifier = nullptr;
    zwlr_foreign_toplevel_manager_v1 *m_manager = nullptr;
    wl_seat *m_seat = nullptr;

    QHash<zwlr_foreign_toplevel_handle_v1 *, Handle *> m_byWl;
    QHash<quint32, Handle *> m_byId;
    quint32 m_nextId = 1;
};
