#include "DesktopIconResolver.h"

#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QProcessEnvironment>
#include <QTextStream>
#include <QPainter>
#include <QFont>
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(logIcon, "dock.icon", QtWarningMsg)

QStringList DesktopIconResolver::searchDirs() const
{
    QStringList dirs;
    const QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    const QString xdgDataHome = env.value("XDG_DATA_HOME");
    if (!xdgDataHome.isEmpty())
        dirs << (xdgDataHome + "/applications");
    else
        dirs << QDir::homePath() + "/.local/share/applications";

    const QString xdgDataDirs = env.value("XDG_DATA_DIRS", "/usr/share:/usr/local/share");
    for (const QString &d : xdgDataDirs.split(':', Qt::SkipEmptyParts))
        dirs << (d + "/applications");
    return dirs;
}

QString DesktopIconResolver::findDesktopFile(const QString &appId) const
{
    if (appId.isEmpty())
        return {};

    const QStringList dirs = searchDirs();
    const QString target = appId + ".desktop";

    // 1. 文件名精确匹配
    for (const QString &dir : dirs) {
        const QString p = dir + "/" + target;
        if (QFile::exists(p))
            return p;
    }

    // 2. 文件名大小写不敏感匹配
    for (const QString &dir : dirs) {
        QDir d(dir);
        if (!d.exists()) continue;
        for (const QString &f : d.entryList(QStringList() << "*.desktop", QDir::Files)) {
            if (f.compare(target, Qt::CaseInsensitive) == 0)
                return dir + "/" + f;
        }
    }

    // 3. StartupWMClass 匹配
    for (const QString &dir : dirs) {
        QDir d(dir);
        if (!d.exists()) continue;
        for (const QString &f : d.entryList(QStringList() << "*.desktop", QDir::Files)) {
            QFile file(dir + "/" + f);
            if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
                continue;
            QTextStream ts(&file);
            for (const QString &line : ts.readAll().split('\n')) {
                if (line.startsWith("StartupWMClass=", Qt::CaseInsensitive)) {
                    const QString val = line.mid(QString("StartupWMClass=").length()).trimmed();
                    if (!val.isEmpty() && val == appId)
                        return dir + "/" + f;
                }
            }
        }
    }
    return {};
}

QString DesktopIconResolver::readIconValue(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return {};
    QTextStream ts(&file);
    bool inDesktopEntry = false;
    for (const QString &line : ts.readAll().split('\n')) {
        if (line.startsWith('[')) {
            inDesktopEntry = (line.trimmed() == "[Desktop Entry]");
            continue;
        }
        if (!inDesktopEntry)
            continue;
        if (line.startsWith("Icon=", Qt::CaseInsensitive))
            return line.mid(5).trimmed();
    }
    return {};
}

QIcon DesktopIconResolver::loadFromIconNameOrPath(const QString &nameOrPath) const
{
    if (nameOrPath.isEmpty())
        return {};
    // 绝对路径
    if (QFileInfo(nameOrPath).isAbsolute()) {
        QPixmap px(nameOrPath);
        if (!px.isNull())
            return QIcon(px);
    }
    // 图标主题名
    const QIcon themed = QIcon::fromTheme(nameOrPath);
    if (!themed.isNull())
        return themed;
    // 也可能是个相对文件名，最后兜底尝试
    if (QFile::exists(nameOrPath)) {
        QPixmap px(nameOrPath);
        if (!px.isNull())
            return QIcon(px);
    }
    return {};
}

QIcon DesktopIconResolver::defaultIcon()
{
    QIcon themed = QIcon::fromTheme("application-x-executable");
    if (!themed.isNull())
        return themed;
    themed = QIcon::fromTheme("applications-other");
    if (!themed.isNull())
        return themed;

    QPixmap px(32, 32);
    px.fill(Qt::transparent);
    QPainter p(&px);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(QColor(220, 220, 220));
    p.setBrush(QColor(80, 80, 90));
    p.drawRoundedRect(2, 2, 28, 28, 4, 4);
    p.setPen(QColor(220, 220, 220));
    QFont f = p.font();
    f.setPointSize(11);
    f.setBold(true);
    p.setFont(f);
    p.drawText(px.rect(), Qt::AlignCenter, QStringLiteral("W"));
    p.end();
    return QIcon(px);
}

QIcon DesktopIconResolver::iconForAppId(const QString &appId)
{
    const auto cacheIt = m_cache.constFind(appId);
    if (cacheIt != m_cache.cend())
        return cacheIt.value();

    QIcon result;
    const QString df = findDesktopFile(appId);
    if (!df.isEmpty()) {
        const QString icon = readIconValue(df);
        result = loadFromIconNameOrPath(icon);
    }
    if (result.isNull()) {
        qCDebug(logIcon) << "no icon found for app_id" << appId << "-> default";
        result = defaultIcon();
    }
    m_cache.insert(appId, result);
    return result;
}
