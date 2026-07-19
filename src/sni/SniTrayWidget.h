#pragma once

#include <QWidget>
#include <QHash>
#include <QSet>
#include <QUuid>

#include "SniTypes.h"

class SniWatcher;
class SniItem;
class SniIconButton;
class QHBoxLayout;

// 系统托盘图标容器：与 TaskManager 同层角色，管理 SniIconButton 列表。
// Passive 状态的 item 不显示（不加入 layout），Active / NeedsAttention 才显示。
class SniTrayWidget : public QWidget
{
    Q_OBJECT
public:
    explicit SniTrayWidget(SniWatcher *watcher, QWidget *parent = nullptr);

public slots:
    void onItemAdded(const QUuid &id, const TrayItemInfo &info);
    void onItemRemoved(const QUuid &id);
    void onItemChanged(const QUuid &id, const TrayItemInfo &info);

signals:
    void sizeChanged(); // dock 用此重绘背景

private:
    SniWatcher *m_watcher;
    QHBoxLayout *m_layout;
    QHash<QUuid, SniIconButton *> m_icons;

    // 记录哪些 id 当前因 Passive 状态被隐藏（不从 layout 中添加）
    QSet<QUuid> m_hiddenIds;
};
