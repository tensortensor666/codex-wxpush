#pragma once

#include "AppSettings.h"

#include <QJsonObject>
#include <QNetworkRequest>
#include <QObject>
#include <QString>

class WxPushClient : public QObject
{
    Q_OBJECT

public:
    explicit WxPushClient(QObject *parent = nullptr);

    bool sendMessage(const AppSettings &settings,
                     const QString &title,
                     const QString &event,
                     const QString &message,
                     const QJsonObject &context,
                     QString *errorMessage);

private:
    bool requestAccessToken(const AppSettings &settings, QString *token, QString *errorMessage);
    bool postTemplateMessage(const AppSettings &settings,
                             const QString &token,
                             const QString &title,
                             const QString &event,
                             const QString &message,
                             const QJsonObject &context,
                             QString *errorMessage);
    QByteArray blockingRequest(QNetworkRequest request,
                               const QByteArray &body,
                               const QByteArray &verb,
                               QString *errorMessage);
    QJsonObject buildTemplateData(const AppSettings &settings,
                                  const QString &title,
                                  const QString &event,
                                  const QString &message,
                                  const QJsonObject &context) const;
    QString buildMessageUrl(const AppSettings &settings,
                            const QString &title,
                            const QString &message) const;
};
