#include "WxPushClient.h"

#include <QEventLoop>
#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QTimer>
#include <QUrl>
#include <QUrlQuery>

namespace {
QString contextValue(const QJsonObject &context, const QString &key)
{
    const QJsonValue value = context.value(key);
    if (value.isString()) {
        return value.toString();
    }
    if (value.isDouble()) {
        return QString::number(value.toDouble());
    }
    if (value.isBool()) {
        return value.toBool() ? QStringLiteral("true") : QStringLiteral("false");
    }
    return {};
}

void replacePlaceholders(QJsonValue &value, const QHash<QString, QString> &replacements)
{
    if (value.isString()) {
        QString text = value.toString();
        for (auto it = replacements.constBegin(); it != replacements.constEnd(); ++it) {
            text.replace(QStringLiteral("{{") + it.key() + QStringLiteral("}}"), it.value());
        }
        value = text;
        return;
    }
    if (value.isObject()) {
        QJsonObject object = value.toObject();
        for (auto it = object.begin(); it != object.end(); ++it) {
            QJsonValue child = it.value();
            replacePlaceholders(child, replacements);
            it.value() = child;
        }
        value = object;
        return;
    }
    if (value.isArray()) {
        QJsonArray array = value.toArray();
        for (int i = 0; i < array.size(); ++i) {
            QJsonValue child = array.at(i);
            replacePlaceholders(child, replacements);
            array[i] = child;
        }
        value = array;
    }
}
}

WxPushClient::WxPushClient(QObject *parent)
    : QObject(parent)
{
}

bool WxPushClient::sendMessage(const AppSettings &settings,
                               const QString &title,
                               const QString &event,
                               const QString &message,
                               const QJsonObject &context,
                               QString *errorMessage)
{
    const QString validation = settings.validationError();
    if (!validation.isEmpty()) {
        if (errorMessage) {
            *errorMessage = validation;
        }
        return false;
    }

    QString token;
    if (!requestAccessToken(settings, &token, errorMessage)) {
        return false;
    }
    return postTemplateMessage(settings, token, title, event, message, context, errorMessage);
}

bool WxPushClient::requestAccessToken(const AppSettings &settings, QString *token, QString *errorMessage)
{
    QUrl url(settings.apiBaseUrl + QStringLiteral("/cgi-bin/token"));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("grant_type"), QStringLiteral("client_credential"));
    query.addQueryItem(QStringLiteral("appid"), settings.appId);
    query.addQueryItem(QStringLiteral("secret"), settings.appSecret);
    url.setQuery(query);

    QNetworkRequest request(url);
    const QByteArray body = blockingRequest(request, {}, QByteArrayLiteral("GET"), errorMessage);
    if (body.isEmpty()) {
        return false;
    }

    const QJsonDocument doc = QJsonDocument::fromJson(body);
    const QJsonObject object = doc.object();
    const QString accessToken = object.value(QStringLiteral("access_token")).toString();
    if (accessToken.isEmpty()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("获取 access_token 失败: %1").arg(QString::fromUtf8(body));
        }
        return false;
    }

    *token = accessToken;
    return true;
}

bool WxPushClient::postTemplateMessage(const AppSettings &settings,
                                       const QString &token,
                                       const QString &title,
                                       const QString &event,
                                       const QString &message,
                                       const QJsonObject &context,
                                       QString *errorMessage)
{
    QUrl url(settings.apiBaseUrl + QStringLiteral("/cgi-bin/message/template/send"));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("access_token"), token);
    url.setQuery(query);

    QJsonObject payload;
    payload.insert(QStringLiteral("touser"), settings.userId);
    payload.insert(QStringLiteral("template_id"), settings.templateId);
    if (!settings.messageUrl.trimmed().isEmpty()) {
        payload.insert(QStringLiteral("url"), buildMessageUrl(settings, title, message));
    }
    payload.insert(QStringLiteral("data"), buildTemplateData(settings, title, event, message, context));

    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json; charset=utf-8"));
    const QByteArray body = blockingRequest(request,
                                           QJsonDocument(payload).toJson(QJsonDocument::Compact),
                                           QByteArrayLiteral("POST"),
                                           errorMessage);
    if (body.isEmpty()) {
        return false;
    }

    const QJsonObject response = QJsonDocument::fromJson(body).object();
    const int errcode = response.value(QStringLiteral("errcode")).toInt(-1);
    if (errcode != 0) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("微信模板消息发送失败: %1").arg(QString::fromUtf8(body));
        }
        return false;
    }

    return true;
}

QByteArray WxPushClient::blockingRequest(QNetworkRequest request,
                                         const QByteArray &body,
                                         const QByteArray &verb,
                                         QString *errorMessage)
{
    QNetworkAccessManager manager;
    QNetworkReply *reply = nullptr;
    if (verb == QByteArrayLiteral("POST")) {
        reply = manager.post(request, body);
    } else {
        reply = manager.get(request);
    }

    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);
    connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
    timer.start(15000);
    loop.exec();

    if (!timer.isActive()) {
        reply->abort();
        reply->deleteLater();
        if (errorMessage) {
            *errorMessage = QStringLiteral("网络请求超时。");
        }
        return {};
    }
    timer.stop();

    const QByteArray response = reply->readAll();
    if (reply->error() != QNetworkReply::NoError) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("网络请求失败: %1 %2").arg(reply->errorString(), QString::fromUtf8(response));
        }
        reply->deleteLater();
        return {};
    }

    reply->deleteLater();
    return response;
}

QJsonObject WxPushClient::buildTemplateData(const AppSettings &settings,
                                            const QString &title,
                                            const QString &event,
                                            const QString &message,
                                            const QJsonObject &context) const
{
    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(settings.templateDataJson.toUtf8(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        doc = QJsonDocument::fromJson(QByteArrayLiteral(R"({"content":{"value":"{{title}}\n{{message}}"}})"));
    }

    QHash<QString, QString> replacements;
    replacements.insert(QStringLiteral("title"), title);
    replacements.insert(QStringLiteral("event"), event);
    replacements.insert(QStringLiteral("message"), message);
    replacements.insert(QStringLiteral("workspace"), contextValue(context, QStringLiteral("workspace")));
    replacements.insert(QStringLiteral("cwd"), contextValue(context, QStringLiteral("cwd")));
    replacements.insert(QStringLiteral("time"), QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")));
    replacements.insert(QStringLiteral("tool"), contextValue(context, QStringLiteral("tool")));
    replacements.insert(QStringLiteral("command"), contextValue(context, QStringLiteral("command")));

    QJsonValue root = doc.object();
    replacePlaceholders(root, replacements);
    return root.toObject();
}

QString WxPushClient::buildMessageUrl(const AppSettings &settings,
                                      const QString &title,
                                      const QString &message) const
{
    QUrl url(settings.messageUrl.trimmed());
    QUrlQuery query(url);
    query.removeAllQueryItems(QStringLiteral("title"));
    query.removeAllQueryItems(QStringLiteral("message"));
    query.removeAllQueryItems(QStringLiteral("date"));
    query.addQueryItem(QStringLiteral("title"), title);
    query.addQueryItem(QStringLiteral("message"), message);
    query.addQueryItem(QStringLiteral("date"), QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")));
    url.setQuery(query);
    return url.toString(QUrl::FullyEncoded);
}
