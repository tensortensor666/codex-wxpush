#include "AppSettings.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSettings>
#include <QStandardPaths>
#include <QTextStream>

namespace {
QString appConfigDir()
{
    QString dir = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    if (dir.isEmpty()) {
        dir = QDir::homePath() + QStringLiteral("/.codex-wxpush");
    }
    QDir().mkpath(dir);
    return dir;
}

QString defaultTemplateJson()
{
    return QStringLiteral(R"({
  "content": { "value": "{{title}}\n{{message}}" },
  "first": { "value": "{{title}}" },
  "keyword1": { "value": "{{event}}" },
  "keyword2": { "value": "{{workspace}}" },
  "keyword3": { "value": "{{time}}" },
  "keyword4": { "value": "{{tool}}" },
  "remark": { "value": "{{message}}\n{{command}}" }
})");
}
}

QString AppSettings::configPath()
{
    return appConfigDir() + QStringLiteral("/config.ini");
}

QString AppSettings::logPath()
{
    return appConfigDir() + QStringLiteral("/codex-wxpush.log");
}

AppSettings AppSettings::load()
{
    AppSettings result;
    result.templateDataJson = defaultTemplateJson();

    QSettings settings(configPath(), QSettings::IniFormat);
    settings.beginGroup(QStringLiteral("general"));
    result.enabled = settings.value(QStringLiteral("enabled"), result.enabled).toBool();
    result.notifyPermissionRequest = settings.value(QStringLiteral("notifyPermissionRequest"), result.notifyPermissionRequest).toBool();
    result.notifyStop = settings.value(QStringLiteral("notifyStop"), result.notifyStop).toBool();
    result.notifyErrors = settings.value(QStringLiteral("notifyErrors"), result.notifyErrors).toBool();
    result.notifyToolUse = settings.value(QStringLiteral("notifyToolUse"), result.notifyToolUse).toBool();
    result.notifyCommands = settings.value(QStringLiteral("notifyCommands"), result.notifyCommands).toBool();
    settings.endGroup();

    settings.beginGroup(QStringLiteral("wechat"));
    result.appId = settings.value(QStringLiteral("appId")).toString().trimmed();
    result.appSecret = settings.value(QStringLiteral("appSecret")).toString().trimmed();
    result.userId = settings.value(QStringLiteral("userId")).toString().trimmed();
    result.templateId = settings.value(QStringLiteral("templateId")).toString().trimmed();
    result.apiBaseUrl = settings.value(QStringLiteral("apiBaseUrl"), result.apiBaseUrl).toString().trimmed();
    result.messageUrl = settings.value(QStringLiteral("messageUrl")).toString().trimmed();
    result.templateDataJson = settings.value(QStringLiteral("templateDataJson"), result.templateDataJson).toString();
    settings.endGroup();

    return result;
}

void AppSettings::save() const
{
    QSettings settings(configPath(), QSettings::IniFormat);
    settings.beginGroup(QStringLiteral("general"));
    settings.setValue(QStringLiteral("enabled"), enabled);
    settings.setValue(QStringLiteral("notifyPermissionRequest"), notifyPermissionRequest);
    settings.setValue(QStringLiteral("notifyStop"), notifyStop);
    settings.setValue(QStringLiteral("notifyErrors"), notifyErrors);
    settings.setValue(QStringLiteral("notifyToolUse"), notifyToolUse);
    settings.setValue(QStringLiteral("notifyCommands"), notifyCommands);
    settings.endGroup();

    settings.beginGroup(QStringLiteral("wechat"));
    settings.setValue(QStringLiteral("appId"), appId.trimmed());
    settings.setValue(QStringLiteral("appSecret"), appSecret.trimmed());
    settings.setValue(QStringLiteral("userId"), userId.trimmed());
    settings.setValue(QStringLiteral("templateId"), templateId.trimmed());
    settings.setValue(QStringLiteral("apiBaseUrl"), apiBaseUrl.trimmed());
    settings.setValue(QStringLiteral("messageUrl"), messageUrl.trimmed());
    settings.setValue(QStringLiteral("templateDataJson"), templateDataJson);
    settings.endGroup();
    settings.sync();
}

bool AppSettings::isPushConfigured() const
{
    return !appId.trimmed().isEmpty()
        && !appSecret.trimmed().isEmpty()
        && !userId.trimmed().isEmpty()
        && !templateId.trimmed().isEmpty()
        && !apiBaseUrl.trimmed().isEmpty();
}

QString AppSettings::validationError() const
{
    if (!enabled) {
        return QStringLiteral("推送已暂停。");
    }
    if (appId.trimmed().isEmpty()) {
        return QStringLiteral("请填写 appid。");
    }
    if (appSecret.trimmed().isEmpty()) {
        return QStringLiteral("请填写 secret。");
    }
    if (userId.trimmed().isEmpty()) {
        return QStringLiteral("请填写接收用户 OpenID。");
    }
    if (templateId.trimmed().isEmpty()) {
        return QStringLiteral("请填写模板 ID。");
    }
    if (apiBaseUrl.trimmed().isEmpty()) {
        return QStringLiteral("请填写微信 API 地址。");
    }
    return {};
}

void appendLog(const QString &message)
{
    QFile file(AppSettings::logPath());
    const QFileInfo info(file);
    QDir().mkpath(info.absolutePath());
    if (!file.open(QIODevice::Append | QIODevice::Text)) {
        return;
    }
    QTextStream stream(&file);
    stream << QDateTime::currentDateTime().toString(Qt::ISODate) << " " << message << '\n';
}
