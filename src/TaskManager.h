#pragma once

#include <QWidget>
#include <QHash>

#include "ForeignToplevelManager.h"

class QHBoxLayout;
class TaskButton;
class DesktopIconResolver;

// 任务管理器：左侧组件，按 toplevel 顺序排列 TaskButton。
// 每个 toplevel 对应一个按钮，不合并；最小化窗口也保留显示。
class TaskManager : public QWidget
{
    Q_OBJECT
public:
    explicit TaskManager(DesktopIconResolver *resolver, QWidget *parent = nullptr);

public slots:
    void onToplevelAdded(quint32 id, const ToplevelInfo &info);
    void onToplevelRemoved(quint32 id);
    void onToplevelChanged(quint32 id, const ToplevelInfo &info);

signals:
    void activateRequested(quint32 id);
    void sizeChanged();

private:
    DesktopIconResolver *m_resolver;
    QHBoxLayout *m_layout;
    QHash<quint32, TaskButton *> m_buttons;
};
