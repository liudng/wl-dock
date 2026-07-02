#include <QApplication>
#include <QLoggingCategory>

#include <LayerShellQt/Shell>

#include "DockController.h"
#include "ForeignToplevelManager.h" // for qRegisterMetaType<ToplevelInfo>

int main(int argc, char *argv[])
{
    qRegisterMetaType<ToplevelInfo>("ToplevelInfo");

    // 必须在 QApplication（及 Wayland plugin）创建之前设置，
    // 否则 Qt Wayland plugin 会用默认 xdg-shell，layer-shell 无法生效
    LayerShellQt::Shell::useLayerShell();

    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("wlroots-dock"));
    QApplication::setApplicationDisplayName(QStringLiteral("wlroots Dock"));
    // 切换 TTY 时所有 output 会被移除再恢复，禁止最后窗口关闭时退出
    QApplication::setQuitOnLastWindowClosed(false);

    DockController controller;
    if (!controller.init()) {
        return 1;
    }

    return app.exec();
}
