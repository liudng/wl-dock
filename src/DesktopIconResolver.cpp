#include "DesktopIconResolver.h"

#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QDirIterator>
#include <QProcessEnvironment>
#include <QTextStream>
#include <QPainter>
#include <QFont>
#include <QLoggingCategory>
#include <QStandardPaths>

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

    // Flatpak 沙箱内 .desktop 兜底路径：当 exports 缺失或未导出时，
    // 直接从安装目录的 files/share/applications 找。
    // 覆盖系统级和用户级两种安装位置。
    const QStringList flatpakRoots = {
        QStringLiteral("/var/lib/flatpak"),
        QDir::homePath() + "/.local/share/flatpak"
    };
    for (const QString &root : flatpakRoots) {
        QDir appDir(root + "/app");
        if (!appDir.exists()) continue;
        for (const QString &sub : appDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
            // <root>/app/<app-id>/current/active/files/share/applications
            const QString p = root + "/app/" + sub +
                              "/current/active/files/share/applications";
            if (QDir(p).exists())
                dirs << p;
        }
    }
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
        if (QFile::exists(p)) {
            qCDebug(logIcon) << "findDesktopFile: exact match" << p;
            return p;
        }
    }

    // 2. 文件名大小写不敏感匹配
    for (const QString &dir : dirs) {
        QDir d(dir);
        if (!d.exists()) continue;
        for (const QString &f : d.entryList(QStringList() << "*.desktop", QDir::Files)) {
            if (f.compare(target, Qt::CaseInsensitive) == 0) {
                qCDebug(logIcon) << "findDesktopFile: case-insensitive match" << dir + "/" + f;
                return dir + "/" + f;
            }
        }
    }

    // 3. StartupWMClass 或 DesktopEntry 内匹配
    for (const QString &dir : dirs) {
        QDir d(dir);
        if (!d.exists()) continue;
        for (const QString &f : d.entryList(QStringList() << "*.desktop", QDir::Files)) {
            const QString path = dir + "/" + f;
            const QString value = readDesktopEntryValue(path, QStringLiteral("StartupWMClass"));
            if (!value.isEmpty() && value == appId) {
                qCDebug(logIcon) << "findDesktopFile: StartupWMClass match" << path;
                return path;
            }
        }
    }

    // 4. Flatpak 应用常常 app_id 是 com.tencent.WeChat 但实际进程
    //    暴露的 app_id 可能是 "wechat" / "WeChat" / 子串等。
    //    用 .desktop 文件里的 X-Flatpak 字段做反向匹配。
    for (const QString &dir : dirs) {
        QDir d(dir);
        if (!d.exists()) continue;
        for (const QString &f : d.entryList(QStringList() << "*.desktop", QDir::Files)) {
            const QString path = dir + "/" + f;
            const QString fp = readDesktopEntryValue(path, QStringLiteral("X-Flatpak"));
            if (!fp.isEmpty() && fp == appId) {
                qCDebug(logIcon) << "findDesktopFile: X-Flatpak match" << path;
                return path;
            }
        }
    }

    // 5. Keywords / Name / Icon 字段反向匹配。
    //    例：com.tencent.WeChat.desktop 里 Keywords=wechat;weixin;，
    //    若进程暴露的 app_id 是 "wechat"，这里能命中。
    //    Name 字段也兜底（大小写不敏感），覆盖 app_id="WeChat" 而
    //    文件名是 com.tencent.WeChat.desktop 且 StartupWMClass 没设的情况。
    const QString appIdLower = appId.toLower();
    for (const QString &dir : dirs) {
        QDir d(dir);
        if (!d.exists()) continue;
        for (const QString &f : d.entryList(QStringList() << "*.desktop", QDir::Files)) {
            const QString path = dir + "/" + f;
            // Keywords 是分号分隔列表
            const QString keywords = readDesktopEntryValue(path, QStringLiteral("Keywords"));
            if (!keywords.isEmpty()) {
                for (const QString &kw : keywords.split(';', Qt::SkipEmptyParts)) {
                    if (kw.trimmed().toLower() == appIdLower) {
                        qCDebug(logIcon) << "findDesktopFile: Keywords match" << path;
                        return path;
                    }
                }
            }
            const QString name = readDesktopEntryValue(path, QStringLiteral("Name"));
            if (!name.isEmpty() && name.toLower() == appIdLower) {
                qCDebug(logIcon) << "findDesktopFile: Name match" << path;
                return path;
            }
            const QString icon = readDesktopEntryValue(path, QStringLiteral("Icon"));
            if (!icon.isEmpty() && icon == appId) {
                qCDebug(logIcon) << "findDesktopFile: Icon match" << path;
                return path;
            }
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

// 在 flatpak 安装目录里手动搜图标。
// 搜两类位置：
//   1) <root>/app/<appId>/current/active/files/share/icons/...    （沙箱内文件）
//   2) <root>/exports/share/icons/...                              （flatpak 自动导出到主机的图标）
// 第二类是 com.tencent.WeChat 这种 .desktop 里 Icon=com.tencent.WeChat 的标准位置：
// /var/lib/flatpak/exports/share/icons/hicolor/256x256/apps/com.tencent.WeChat.png
// 当 Qt 的 QIcon::fromTheme 因 themeSearchPaths 缺失 / 主题没刷新 / 命名约定不匹配而失败时，这里直接遍历文件系统兜底。
static QIcon findIconInFlatpakApp(const QString &appId, const QString &iconName)
{
    const QString name = iconName.isEmpty() ? appId : iconName;
    const QStringList roots = {
        QStringLiteral("/var/lib/flatpak"),
        QDir::homePath() + "/.local/share/flatpak"
    };
    QStringList searchBases;
    for (const QString &root : roots) {
        // 沙箱内
        searchBases << root + "/app/" + appId + "/current/active/files/share/icons";
        // flatpak 自动导出（与 appId 无关，是导出到主机的全局 icons 目录）
        searchBases << root + "/exports/share/icons";
    }

    for (const QString &base : searchBases) {
        if (!QDir(base).exists()) continue;
        QDirIterator it(base, QStringList() << name + ".png" << name + ".svg"
                                            << name + ".xpm",
                        QDir::Files, QDirIterator::Subdirectories);
        QString best;
        int bestScore = -1;
        while (it.hasNext()) {
            const QString p = it.next();
            // 偏好较大尺寸（48 / 64 / 128 / 256）
            QFileInfo fi(p);
            bool ok = false;
            int sz = fi.dir().dirName().toInt(&ok);
            if (!ok) sz = 0;
            if (sz > bestScore) {
                bestScore = sz;
                best = p;
            }
        }
        if (!best.isEmpty()) {
            qCDebug(logIcon) << "findIconInFlatpakApp: hit" << best;
            return QIcon(best);
        }
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
    QString iconName;
    if (!df.isEmpty()) {
        iconName = readIconValue(df);
        result = loadFromIconNameOrPath(iconName);
    }

    // Flatpak 应用兜底：.desktop 里 Icon 字段（通常等于 appId）在主机图标主题里没有
    // 时，直接进 flatpak 沙箱内的 icons 目录手动找。
    if (result.isNull() && !appId.isEmpty()) {
        const QIcon fp = findIconInFlatpakApp(appId, iconName);
        if (!fp.isNull())
            result = fp;
    }

    // 最后兜底：直接以 app_id 作为图标名查主题。
    // flatpak / GLib 应用经常图标名 == app_id（如 com.tencent.WeChat），
    // 而 .desktop 文件丢失或 Icon 字段空时这里仍能命中。
    if (result.isNull() && !appId.isEmpty()) {
        const QIcon byId = QIcon::fromTheme(appId);
        if (!byId.isNull()) {
            qCDebug(logIcon) << "iconForAppId: fall back to theme icon by appId" << appId;
            result = byId;
        }
    }

    if (result.isNull()) {
        qCWarning(logIcon) << "no icon found for app_id" << appId
                           << "| .desktop=" << df
                           << "| Icon=" << iconName
                           << "-> default";
        result = defaultIcon();
    } else {
        qCDebug(logIcon) << "iconForAppId: resolved" << appId
                         << "| .desktop=" << df
                         << "| Icon=" << iconName;
    }
    m_cache.insert(appId, result);
    return result;
}
