#pragma once

#include <QHash>
#include <QIcon>
#include <QString>
#include <QStringList>

// 根据 foreign-toplevel 暴露的 app_id 反查 .desktop 文件，
// 解析其中的 Icon= 字段获取图标；查不到时返回一个默认图标。
class DesktopIconResolver
{
public:
    QIcon iconForAppId(const QString &appId);

private:
    QStringList searchDirs() const;
    QString findDesktopFile(const QString &appId) const;
    static QString readDesktopEntryValue(const QString &path, const QString &key);
    static QString readIconValue(const QString &path);
    QIcon loadFromIconNameOrPath(const QString &nameOrPath) const;
    static QIcon defaultIcon();

    QHash<QString, QIcon> m_cache;
};
