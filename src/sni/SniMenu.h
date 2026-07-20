#pragma once

#include <QWidget>
#include <QList>
#include <QIcon>
#include <QString>

class QEventLoop;
class QWindow;
class QTimer;

/**
 * 自绘 SNI 托盘右键菜单。
 *
 * 不用 QMenu 的原因：LayerShellQt 会把所有 Qt::Popup 顶层窗口 attach 为 layer popup，
 * 但 layer-shell 协议不支持 popup，attach 失败（"Cannot attach popup of unknown type"），
 * 退回 xdg_popup 后位置算到屏幕中央。
 *
 * 此类用 Qt::Window flag 让 LayerShellQt 当作普通 layer_surface attach，
 * 然后通过 LayerShellQt::Window 设置 anchors=Top|Left + margins=screenPos 精确定位。
 */
class SniMenu : public QWidget
{
    Q_OBJECT
public:
    struct Item
    {
        QString label;
        QIcon icon;
        bool enabled = true;
        bool separator = false;
        bool checkable = false;
        bool checked = false;
        int id = -1; // DBusMenu item id
    };

    explicit SniMenu(QWidget *parent = nullptr);

    void setItems(const QList<Item> &items);

    // 在鼠标位置弹出菜单（同步阻塞）。返回被点击 item 的 id，未选返回 -1。
    // dockWindow 是触发菜单的 dock 顶层窗口，用于计算屏幕实际坐标
    // （Qt Wayland 客户端不知道自己 surface 的全局位置，需要补偿）。
    int popup(QWindow *dockWindow, const QPoint &globalPos);

protected:
    void paintEvent(QPaintEvent *e) override;
    void mouseMoveEvent(QMouseEvent *e) override;
    void mousePressEvent(QMouseEvent *e) override;
    void keyPressEvent(QKeyEvent *e) override;
    void hideEvent(QHideEvent *e) override;
    void leaveEvent(QEvent *e) override;
    bool eventFilter(QObject *o, QEvent *e) override;
    QSize sizeHint() const override;

private:
    int itemAtY(int y) const;
    int itemTop(int index) const;
    int itemHeight(int index) const;
    void updateHover(int y);
    void moveHover(int delta);

    QList<Item> m_items;
    int m_hovered = -1;
    int m_result = -1;
    QEventLoop *m_loop = nullptr;
    QTimer *m_outsideTimer = nullptr; // 鼠标移出菜单区域延迟关闭
};
