#include "ClockWidget.h"

#include <QLabel>
#include <QVBoxLayout>
#include <QTimer>
#include <QDateTime>
#include <QTime>
#include <QLocale>

ClockWidget::ClockWidget(QWidget *parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_TranslucentBackground);

    auto *l = new QVBoxLayout(this);
    l->setContentsMargins(2, 0, 2, 0);
    l->setSpacing(0);
    l->setAlignment(Qt::AlignCenter);

    m_time = new QLabel(this);
    m_time->setAlignment(Qt::AlignCenter);
    QFont tf = m_time->font();
    tf.setPointSize(14);
    tf.setBold(true);
    m_time->setFont(tf);
    m_time->setStyleSheet(QStringLiteral("color: #ffffff; background: transparent;"));

    m_date = new QLabel(this);
    m_date->setAlignment(Qt::AlignCenter);
    QFont df = m_date->font();
    df.setPointSize(8);
    m_date->setFont(df);
    m_date->setStyleSheet(QStringLiteral("color: #bbbbbb; background: transparent;"));

    l->addWidget(m_time);
    l->addWidget(m_date);

    m_timer = new QTimer(this);
    m_timer->setSingleShot(true);
    connect(m_timer, &QTimer::timeout, this, &ClockWidget::refresh);
    refresh();
}

void ClockWidget::refresh()
{
    const QDateTime now = QDateTime::currentDateTime();
    m_time->setText(now.toString("HH:mm"));

    // 下行：YYYY/MM/DD 中文星期几
    static const char *kDays[] = {
        "\xe6\x98\x9f\xe6\x9c\x9f\xe4\xb8\x80", // 星期一
        "\xe6\x98\x9f\xe6\x9c\x9f\xe4\xba\x8c", // 星期二
        "\xe6\x98\x9f\xe6\x9c\x9f\xe4\xb8\x89", // 星期三
        "\xe6\x98\x9f\xe6\x9c\x9f\xe5\x9b\x9b", // 星期四
        "\xe6\x98\x9f\xe6\x9c\x9f\xe4\xba\x94", // 星期五
        "\xe6\x98\x9f\xe6\x9c\x9f\xe5\x85\xad", // 星期六
        "\xe6\x98\x9f\xe6\x9c\x9f\xe6\x97\xa5"  // 星期日
    };
    const int dow = now.date().dayOfWeek(); // 1=Mon .. 7=Sun
    QString dateStr = now.toString("yyyy/MM/dd ");
    if (dow >= 1 && dow <= 7)
        dateStr += QString::fromUtf8(kDays[dow - 1]);
    m_date->setText(dateStr);

    scheduleNextMinute();
}

void ClockWidget::scheduleNextMinute()
{
    const QDateTime now = QDateTime::currentDateTime();
    // 计算到下一个整分钟的毫秒数
    const int sec = now.time().second();
    const int msec = now.time().msec();
    qint64 msToNext = (60 - sec) * 1000 - msec;
    if (msToNext <= 0) msToNext = 1000;
    m_timer->start(int(msToNext));
}
