#pragma once

#include <QIcon>
#include <QString>
#include <QMetaType>
#include <QLoggingCategory>

Q_DECLARE_LOGGING_CATEGORY(logSni)

// 单个 SNI 托盘项的快照，跨线程经信号传递。
// 由 SniItem 的属性快照构建，给 UI 层使用。
struct TrayItemInfo {
    QString id;            // SNI Id（应用标识）
    QString title;         // Title
    QString status;        // "Passive" / "Active" / "NeedsAttention"
    QIcon   icon;          // 由 IconName / IconPixmap / Attention* 综合回退合成
    QString tooltipText;   // ToolTip 结构里的 title + subtitle
    bool    itemIsMenu = false; // true ⇒ 只支持菜单，不支持 Activate
};

Q_DECLARE_METATYPE(TrayItemInfo)
