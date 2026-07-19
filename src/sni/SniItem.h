#pragma once

#include <QObject>
#include <QString>
#include <QIcon>
#include <QList>
#include <QByteArray>

#include "SniTypes.h"

class QDBusInterface;

// 单个 StatusNotifierItem（系统托盘应用）的 D-Bus 代理。
// 一个 SniItem 对应一个托盘应用。它：
//   - GetAll 读初始属性
//   - 订阅 NewIcon / NewTitle / NewStatus / NewToolTip 信号，到达后重读对应属性
//   - 提供 activate() / secondaryActivate() 调用远端方法
//   - 解码 SNI 的 a(iiay) 大端 ARGB32 pixmap → QIcon
//
// 协议本身不依赖 KDE 运行时，仅是 D-Bus 接口名带 org.kde. 前缀（历史遗留）。
// labwc / sway / Hyprland / GNOME 通用，前提是 dbus-session 在跑。
class SniItem : public QObject
{
    Q_OBJECT
public:
    explicit SniItem(const QString &service, const QString &path, QObject *parent = nullptr);
    ~SniItem() override;

    // 初始化：拉取全部属性，订阅信号。返回 false 表示远端不可达。
    bool init();

    const QString &service() const { return m_service; }
    const QString &path() const { return m_path; }

    // 构建当前快照（每次属性变更后由 watcher 调用）
    TrayItemInfo snapshot() const;

    // 触发远端动作。Wayland 下全局坐标不可知，按惯例传 (0,0)。
    void activate(int x = 0, int y = 0);
    void secondaryActivate(int x = 0, int y = 0);

signals:
    // 任意属性变化时触发，watcher 转发给 UI 层。
    void changed(SniItem *self);
    // 远端 service 从总线消失（应用退出）
    void gone(SniItem *self);

private slots:
    void onNewIcon();
    void onNewTitle();
    void onNewStatus();
    void onNewToolTip();
    void onServiceOwnerChanged(const QString &name, const QString &oldOwner, const QString &newOwner);

private:
    struct PixmapEntry { int width = 0; int height = 0; QByteArray bytes; };

    void refreshAll();
    // 异步读属性：拿到值后回调（UI 线程内的 Qt 事件循环）
    void readPropertyAsync(const char *method, std::function<void(const QVariant&)> cb);

    QIcon buildIcon() const;
    static QIcon fromIconName(const QString &name);
    static QIcon fromPixmapArray(const QList<PixmapEntry> &pixmaps);
    static QList<PixmapEntry> decodePixmapArray(const QVariant &v);
    static QString toolTipToString(const QVariant &v);

    QString m_service;
    QString m_path;
    QDBusInterface *m_iface = nullptr;

    // 缓存的原始属性
    QString m_id;
    QString m_title;
    QString m_status;        // "Passive"/"Active"/"NeedsAttention"
    QString m_iconName;
    QList<PixmapEntry> m_iconPixmap;
    QString m_attentionIconName;
    QList<PixmapEntry> m_attentionIconPixmap;
    QString m_tooltipText;
    bool m_itemIsMenu = false;
};
