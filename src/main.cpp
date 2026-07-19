#include <QApplication>
#include <QLoggingCategory>
#include <QTextStream>
#include <QString>

#include <LayerShellQt/Shell>

#include "DockController.h"
#include "ForeignToplevelManager.h" // for qRegisterMetaType<ToplevelInfo>
#include "sni/SniTypes.h"             // for qRegisterMetaType<TrayItemInfo>

#ifndef WL_DOCK_VERSION
#define WL_DOCK_VERSION "unknown"
#endif

int main(int argc, char *argv[])
{
    // --version / -v：在 QApplication 创建之前处理，避免无谓初始化 Wayland
    for (int i = 1; i < argc; ++i) {
        const QString arg = QString::fromLocal8Bit(argv[i]);
        if (arg == QStringLiteral("--version") || arg == QStringLiteral("-v")) {
            QTextStream(stdout) << QStringLiteral("wl-dock %1").arg(QStringLiteral(WL_DOCK_VERSION)) << Qt::endl;
            return 0;
        }
    }

    qRegisterMetaType<ToplevelInfo>("ToplevelInfo");
    qRegisterMetaType<TrayItemInfo>("TrayItemInfo");

    // 必须在 QApplication（及 Wayland plugin）创建之前设置，
    // 否则 Qt Wayland plugin 会用默认 xdg-shell，layer-shell 无法生效
    LayerShellQt::Shell::useLayerShell();

    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("wl-dock"));
    QApplication::setApplicationDisplayName(QStringLiteral("wl-dock"));
    QApplication::setApplicationVersion(QStringLiteral(WL_DOCK_VERSION));
    // 切换 TTY 时所有 output 会被移除再恢复，禁止最后窗口关闭时退出
    QApplication::setQuitOnLastWindowClosed(false);

    DockController controller;
    if (!controller.init()) {
        return 1;
    }

    return app.exec();
}
