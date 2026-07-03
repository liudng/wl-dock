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

static bool isDesktopLineComment(const QString &line)
{
    const QString trimmed = line.trimmed();
    return trimmed.startsWith('#') || trimmed.startsWith(';');
}

static QString desktopEntryKey(const QString &line)
{
    const int eq = line.indexOf('=');
    if (eq <= 0)
        return {};
    return line.left(eq).trimmed();
}

static QString desktopEntryValue(const QString &line)
{
    const int eq = line.indexOf('=');
    if (eq < 0)
        return {};
    return line.mid(eq + 1).trimmed();
}

QString DesktopIconResolver::findDesktopFile(const QString &appId) const
{
    if (appId.isEmpty())
        return {};

    QStringList dirs = searchDirs();
    dirs.removeDuplicates();
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

    // 3. StartupWMClass 或 DesktopEntry 内匹配
    for (const QString &dir : dirs) {
        QDir d(dir);
        if (!d.exists()) continue;
        for (const QString &f : d.entryList(QStringList() << "*.desktop", QDir::Files)) {
            const QString path = dir + "/" + f;
            const QString value = readDesktopEntryValue(path, QStringLiteral("StartupWMClass"));
            if (!value.isEmpty() && value == appId)
                return path;
        }
    }
    return {};
}

QString DesktopIconResolver::readDesktopEntryValue(const QString &path, const QString &key)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return {};
    QTextStream ts(&file);
    bool inDesktopEntry = false;
    while (!ts.atEnd()) {
        const QString line = ts.readLine();
        if (line.trimmed().isEmpty() || isDesktopLineComment(line))
            continue;
        const QString trimmed = line.trimmed();
        if (trimmed.startsWith('[') && trimmed.endsWith(']')) {
            inDesktopEntry = (trimmed.compare("[Desktop Entry]", Qt::CaseInsensitive) == 0);
            continue;
        }
        if (!inDesktopEntry)
            continue;
        const QString name = desktopEntryKey(trimmed);
        if (name.isEmpty())
            continue;
        if (name.compare(key, Qt::CaseInsensitive) == 0)
            return desktopEntryValue(trimmed);
    }
    return {};
}

QString DesktopIconResolver::readIconValue(const QString &path)
{
    return readDesktopEntryValue(path, QStringLiteral("Icon"));
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
