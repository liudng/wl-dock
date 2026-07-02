#pragma once

#include <QWidget>

class QLabel;
class QTimer;

// 时钟组件：右侧固定宽度。
// 上行较大字体显示 HH:MM（24 小时制）；
// 下行较小字体显示 YYYY/MM/DD 中文星期几。
// 刷新对齐到分钟边界，保证显示的分钟数准确。
class ClockWidget : public QWidget
{
    Q_OBJECT
public:
    explicit ClockWidget(QWidget *parent = nullptr);

private slots:
    void refresh();

private:
    void scheduleNextMinute();

    QLabel *m_time = nullptr;
    QLabel *m_date = nullptr;
    QTimer *m_timer = nullptr;
};
