#include "HookInstaller.h"

#include "AppSettings.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QTextStream>

namespace {
QString windowsToWslPath(QString path)
{
    path.replace(QLatin1Char('\\'), QLatin1Char('/'));
    if (path.size() >= 2 && path.at(1) == QLatin1Char(':')) {
        const QChar drive = path.at(0).toLower();
        path = QStringLiteral("/mnt/") + drive + path.mid(2);
    }
    return path;
}

QJsonObject hookCommandObject(const QString &exePath, int timeout)
{
    QJsonObject hook;
    hook.insert(QStringLiteral("type"), QStringLiteral("command"));
    hook.insert(QStringLiteral("command"), QStringLiteral("\"%1\" --hook").arg(windowsToWslPath(exePath)));
    hook.insert(QStringLiteral("commandWindows"), QStringLiteral("& \"%1\" --hook").arg(exePath));
    hook.insert(QStringLiteral("timeout"), timeout);
    return hook;
}

QString hookExecutablePath()
{
    const QFileInfo current(QCoreApplication::applicationFilePath());
    if (current.completeBaseName().endsWith(QStringLiteral("-hook"), Qt::CaseInsensitive)) {
        return QDir::toNativeSeparators(current.absoluteFilePath());
    }

    const QString helper = current.absoluteDir().absoluteFilePath(QStringLiteral("codex-wxpush-hook.exe"));
    if (QFileInfo::exists(helper)) {
        return QDir::toNativeSeparators(helper);
    }
    return QDir::toNativeSeparators(current.absoluteFilePath());
}

QJsonArray hookArray(const QString &exePath, int timeout)
{
    QJsonObject item;
    item.insert(QStringLiteral("hooks"), QJsonArray{hookCommandObject(exePath, timeout)});
    return QJsonArray{item};
}

bool isOwnHook(const QJsonObject &hook)
{
    const QString command = hook.value(QStringLiteral("command")).toString()
        + QLatin1Char('\n')
        + hook.value(QStringLiteral("commandWindows")).toString();
    return command.contains(QStringLiteral("codex-wxpush"), Qt::CaseInsensitive)
        && command.contains(QStringLiteral("--hook"), Qt::CaseInsensitive);
}

QJsonArray withoutOwnHooks(QJsonArray eventEntries)
{
    QJsonArray cleanedEntries;
    for (const QJsonValue &entryValue : eventEntries) {
        QJsonObject entry = entryValue.toObject();
        const QJsonArray hooks = entry.value(QStringLiteral("hooks")).toArray();
        QJsonArray cleanedHooks;
        for (const QJsonValue &hookValue : hooks) {
            const QJsonObject hook = hookValue.toObject();
            if (!isOwnHook(hook)) {
                cleanedHooks.append(hook);
            }
        }
        if (!cleanedHooks.isEmpty()) {
            entry.insert(QStringLiteral("hooks"), cleanedHooks);
            cleanedEntries.append(entry);
        }
    }
    return cleanedEntries;
}

QJsonArray withOwnHook(QJsonArray eventEntries, const QString &exePath, int timeout)
{
    eventEntries = withoutOwnHooks(eventEntries);
    const QJsonArray own = hookArray(exePath, timeout);
    for (const QJsonValue &value : own) {
        eventEntries.append(value);
    }
    return eventEntries;
}

bool ensureHooksFeature(QString *errorMessage)
{
    QFile file(HookInstaller::configPath());
    if (!file.exists()) {
        QDir().mkpath(QFileInfo(file).absolutePath());
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            if (errorMessage) {
                *errorMessage = QStringLiteral("无法创建 Codex config.toml: %1").arg(file.errorString());
            }
            return false;
        }
        QTextStream(&file) << "[features]\nhooks = true\n";
        return true;
    }

    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("无法读取 Codex config.toml: %1").arg(file.errorString());
        }
        return false;
    }
    QString text = QString::fromUtf8(file.readAll());
    file.close();

    if (QRegularExpression(QStringLiteral(R"((?m)^\s*hooks\s*=\s*true\s*$)")).match(text).hasMatch()) {
        return true;
    }

    const QRegularExpression featuresHeader(QStringLiteral(R"((?m)^\[features\]\s*$)"));
    const QRegularExpressionMatch match = featuresHeader.match(text);
    if (match.hasMatch()) {
        const int insertAt = match.capturedEnd();
        text.insert(insertAt, QStringLiteral("\nhooks = true"));
    } else {
        if (!text.endsWith(QLatin1Char('\n'))) {
            text.append(QLatin1Char('\n'));
        }
        text.append(QStringLiteral("\n[features]\nhooks = true\n"));
    }

    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("无法写入 Codex config.toml: %1").arg(file.errorString());
        }
        return false;
    }
    QTextStream(&file) << text;
    return true;
}
}

QString HookInstaller::codexDir()
{
    return QDir::homePath() + QStringLiteral("/.codex");
}

QString HookInstaller::hooksPath()
{
    return codexDir() + QStringLiteral("/hooks.json");
}

QString HookInstaller::configPath()
{
    return codexDir() + QStringLiteral("/config.toml");
}

bool HookInstaller::install(QString *errorMessage)
{
    if (!ensureHooksFeature(errorMessage)) {
        return false;
    }

    QDir().mkpath(codexDir());
    const QString exePath = hookExecutablePath();

    QJsonObject root;
    QFile file(hooksPath());
    if (file.exists() && file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        root = QJsonDocument::fromJson(file.readAll()).object();
        file.close();
    }

    QJsonObject hooks = root.value(QStringLiteral("hooks")).toObject();
    hooks.insert(QStringLiteral("PermissionRequest"),
                 withOwnHook(hooks.value(QStringLiteral("PermissionRequest")).toArray(), exePath, 60));
    hooks.insert(QStringLiteral("Stop"),
                 withOwnHook(hooks.value(QStringLiteral("Stop")).toArray(), exePath, 60));
    hooks.insert(QStringLiteral("PreToolUse"),
                 withOwnHook(hooks.value(QStringLiteral("PreToolUse")).toArray(), exePath, 60));
    hooks.insert(QStringLiteral("PostToolUse"),
                 withOwnHook(hooks.value(QStringLiteral("PostToolUse")).toArray(), exePath, 60));
    root.insert(QStringLiteral("hooks"), hooks);

    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("无法写入 hooks.json: %1").arg(file.errorString());
        }
        return false;
    }

    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    appendLog(QStringLiteral("installed hooks to %1").arg(hooksPath()));
    return true;
}

bool HookInstaller::uninstall(QString *errorMessage)
{
    QFile file(hooksPath());
    if (!file.exists()) {
        return true;
    }
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("无法读取 hooks.json: %1").arg(file.errorString());
        }
        return false;
    }

    QJsonObject root = QJsonDocument::fromJson(file.readAll()).object();
    file.close();
    QJsonObject hooks = root.value(QStringLiteral("hooks")).toObject();
    hooks.insert(QStringLiteral("PermissionRequest"),
                 withoutOwnHooks(hooks.value(QStringLiteral("PermissionRequest")).toArray()));
    hooks.insert(QStringLiteral("Stop"),
                 withoutOwnHooks(hooks.value(QStringLiteral("Stop")).toArray()));
    hooks.insert(QStringLiteral("PreToolUse"),
                 withoutOwnHooks(hooks.value(QStringLiteral("PreToolUse")).toArray()));
    hooks.insert(QStringLiteral("PostToolUse"),
                 withoutOwnHooks(hooks.value(QStringLiteral("PostToolUse")).toArray()));
    root.insert(QStringLiteral("hooks"), hooks);

    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("无法写入 hooks.json: %1").arg(file.errorString());
        }
        return false;
    }

    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    appendLog(QStringLiteral("uninstalled hooks from %1").arg(hooksPath()));
    return true;
}
