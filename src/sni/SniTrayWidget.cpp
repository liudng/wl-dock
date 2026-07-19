#include "SniTrayWidget.h"
#include "SniWatcher.h"
#include "SniItem.h"
#include "SniIconButton.h"

#include <QHBoxLayout>


SniTrayWidget::SniTrayWidget(SniWatcher *watcher, QWidget *parent)
    : QWidget(parent)
    , m_watcher(watcher)
{
    setAttribute(Qt::WA_TranslucentBackground);
    m_layout = new QHBoxLayout(this);
    m_layout->setContentsMargins(0, 0, 0, 0);
    m_layout->setSpacing(0);
}

void SniTrayWidget::onItemAdded(const QUuid &id, const TrayItemInfo &info)
{
    if (m_icons.contains(id)) {
        onItemChanged(id, info);
        return;
    }

    SniItem *item = m_watcher->items().value(id);
    if (!item) return;

    auto *btn = new SniIconButton(item, this);
    btn->setTrayIcon(info.icon);
    btn->setToolTip(info.tooltipText.isEmpty() ? info.id : info.tooltipText);
    btn->setAttention(info.status == QLatin1String("NeedsAttention"));

    connect(btn, &SniIconButton::activateRequested, this, [](SniItem *i) {
        i->activate(0, 0);
    });
    connect(btn, &SniIconButton::secondaryActivateRequested, this, [](SniItem *i) {
        i->secondaryActivate(0, 0);
    });

    m_icons.insert(id, btn);

    // Passive 状态不显示
    if (info.status == QLatin1String("Passive")) {
        m_hiddenIds.insert(id);
        return;
    }

    m_layout->addWidget(btn);
    emit sizeChanged();
}

void SniTrayWidget::onItemRemoved(const QUuid &id)
{
    const auto it = m_icons.find(id);
    if (it == m_icons.end()) return;

    SniIconButton *btn = it.value();
    m_hiddenIds.remove(id);
    m_layout->removeWidget(btn);
    delete btn;
    m_icons.erase(it);
    emit sizeChanged();
}

void SniTrayWidget::onItemChanged(const QUuid &id, const TrayItemInfo &info)
{
    const auto it = m_icons.find(id);
    if (it == m_icons.end()) {
        onItemAdded(id, info);
        return;
    }
    SniIconButton *btn = it.value();

    btn->setTrayIcon(info.icon);
    btn->setToolTip(info.tooltipText.isEmpty() ? info.id : info.tooltipText);
    btn->setAttention(info.status == QLatin1String("NeedsAttention"));

    const bool wasHidden = m_hiddenIds.contains(id);
    const bool shouldHide = (info.status == QLatin1String("Passive"));

    if (wasHidden && !shouldHide) {
        // 从隐藏恢复为显示
        m_hiddenIds.remove(id);
        m_layout->addWidget(btn);
        emit sizeChanged();
    } else if (!wasHidden && shouldHide) {
        // 从显示变为隐藏
        m_hiddenIds.insert(id);
        m_layout->removeWidget(btn);
        btn->setParent(this); // 保持所有权，不从 layout 中显示
        emit sizeChanged();
    }
}
