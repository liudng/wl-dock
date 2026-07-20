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

    // 设置默认应用图标名称（查不到 .desktop 时的兜底图标），
    // 默认 "application-x-executable"。可通过命令行 --default-icon 指定。
    void setDefaultIconName(const QString &name) { m_defaultIconName = name; }

private:
    QStringList searchDirs() const;
    QString findDesktopFile(const QString &appId) const;
    static QString readDesktopEntryValue(const QString &path, const QString &key);
    static QString readIconValue(const QString &path);
    QIcon loadFromIconNameOrPath(const QString &nameOrPath) const;
    QIcon defaultIcon();

    QHash<QString, QIcon> m_cache;
    QString m_defaultIconName = QStringLiteral("application-x-executable");
};
