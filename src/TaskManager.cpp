#include "TaskManager.h"

#include "TaskButton.h"
#include "DesktopIconResolver.h"

#include <QHBoxLayout>

TaskManager::TaskManager(DesktopIconResolver *resolver, QWidget *parent)
    : QWidget(parent)
    , m_resolver(resolver)
{
    setAttribute(Qt::WA_TranslucentBackground);
    m_layout = new QHBoxLayout(this);
    m_layout->setContentsMargins(0, 0, 0, 0);
    m_layout->setSpacing(4);
}

void TaskManager::onToplevelAdded(quint32 id, const ToplevelInfo &info)
{
    if (m_buttons.contains(id)) {
        onToplevelChanged(id, info);
        return;
    }
    auto btn = new TaskButton(this);
    btn->setAppIcon(m_resolver->iconForAppId(info.appId));
    btn->setToolTip(info.title.isEmpty() ? info.appId : info.title);
    btn->setActive(info.activated);
    connect(btn, &TaskButton::clicked, this, [this, id] { emit activateRequested(id); });
    m_layout->addWidget(btn);
    m_buttons.insert(id, btn);
    emit sizeChanged();
}

void TaskManager::onToplevelRemoved(quint32 id)
{
    const auto it = m_buttons.find(id);
    if (it == m_buttons.end()) return;
    TaskButton *btn = it.value();
    m_layout->removeWidget(btn);
    delete btn;
    m_buttons.erase(it);
    emit sizeChanged();
}

void TaskManager::onToplevelChanged(quint32 id, const ToplevelInfo &info)
{
    const auto it = m_buttons.find(id);
    if (it == m_buttons.end()) {
        onToplevelAdded(id, info);
        return;
    }
    TaskButton *btn = it.value();
    btn->setToolTip(info.title.isEmpty() ? info.appId : info.title);
    btn->setActive(info.activated);
    btn->setAppIcon(m_resolver->iconForAppId(info.appId));
}
