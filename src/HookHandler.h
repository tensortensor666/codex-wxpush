#pragma once

#include "AppSettings.h"

#include <QJsonObject>
#include <QString>

class HookHandler
{
public:
    int run(const QStringList &arguments);

private:
    QString readStdin() const;
    QJsonObject parsePayload(const QString &text) const;
    QString eventName(const QJsonObject &payload, const QStringList &arguments) const;
    QString notificationTitle(const QString &event, const QJsonObject &payload) const;
    QString buildMessage(const QString &event, const QJsonObject &payload) const;
    QJsonObject contextObject(const QJsonObject &payload) const;
    bool shouldNotify(const AppSettings &settings, const QString &event, const QJsonObject &payload) const;
};
