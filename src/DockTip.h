#pragma once

#include <QWidget>

// 自绘 tooltip：作为 dock 的子 QWidget 渲染，不创建独立顶层窗口。
// 原因：dock 是 layer-surface + 自动隐藏（高度在 1px/64px 间切换），
// 每次高度变化触发 labwc layer-surface reconfigure，产生虚假 leave。
// Qt 原生 QToolTip 依赖合成器创建 popup，且其显示延时定时器在 leave
// 时会被重置 —— 在 layer-shell 场景下要么位置错乱，要么永远显示不出来。
// 这里自绘为 dock 子控件：不开新窗口、用按钮局部坐标做锚点、自己控制
// 延时与隐藏，彻底避开 leave 风暴与 popup 定位问题。
class DockTip : public QWidget
{
    Q_OBJECT
public:
    explicit DockTip(QWidget *parent = nullptr);

    // 在 anchor 上方居中显示 text。多次调用会复用同一个实例并重新定位。
    void showTip(QWidget *anchor, const QString &text);

    void hideTip();

protected:
    void paintEvent(QPaintEvent *e) override;

private:
    QString m_text;
};
