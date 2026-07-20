#include <QApplication>
#include <QIcon>
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

static void printHelp()
{
    QTextStream out(stdout);
    out << QStringLiteral("Usage: wl-dock [options]\n\n"
                          "Options:\n"
                          "  -v, --version            Show version and exit\n"
                          "  -h, --help               Show this help and exit\n"
                          "      --icon-theme <name>  Set icon theme name (e.g. Adwaita, Breeze, Papirus)\n"
                          "      --default-icon <name>  Set fallback app icon name when .desktop lookup fails\n"
                          "                            (default: application-x-executable)\n");
}

int main(int argc, char *argv[])
{
    // 在 QApplication 创建之前解析参数，避免无谓初始化 Wayland（--version/--help）
    QString iconTheme;
    QString defaultIconName;
    for (int i = 1; i < argc; ++i) {
        const QString arg = QString::fromLocal8Bit(argv[i]);
        if (arg == QStringLiteral("--version") || arg == QStringLiteral("-v")) {
            QTextStream(stdout) << QStringLiteral("wl-dock %1").arg(QStringLiteral(WL_DOCK_VERSION)) << Qt::endl;
            return 0;
        } else if (arg == QStringLiteral("--help") || arg == QStringLiteral("-h")) {
            printHelp();
            return 0;
        } else if (arg == QStringLiteral("--icon-theme") && i + 1 < argc) {
            iconTheme = QString::fromLocal8Bit(argv[++i]);
        } else if (arg == QStringLiteral("--default-icon") && i + 1 < argc) {
            defaultIconName = QString::fromLocal8Bit(argv[++i]);
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

    // 设置图标主题（影响 QIcon::fromTheme 的查找路径）
    if (!iconTheme.isEmpty())
        QIcon::setThemeName(iconTheme);

    DockController controller;
    if (!controller.init(defaultIconName)) {
        return 1;
    }

    return app.exec();
}
