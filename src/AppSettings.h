#pragma once

#include <QDateTime>
#include <QJsonObject>
#include <QString>

class AppSettings
{
public:
    bool enabled = true;
    bool notifyPermissionRequest = true;
    bool notifyStop = true;
    bool notifyErrors = true;
    bool notifyToolUse = false;
    bool notifyCommands = false;

    QString appId;
    QString appSecret;
    QString userId;
    QString templateId;
    QString apiBaseUrl = QStringLiteral("https://api.weixin.qq.com");
    QString messageUrl;
    QString templateDataJson;

    static QString configPath();
    static QString logPath();
    static AppSettings load();

    void save() const;
    bool isPushConfigured() const;
    QString validationError() const;
};

void appendLog(const QString &message);
