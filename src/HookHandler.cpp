#include "HookHandler.h"

#include "WxPushClient.h"

#include <QCoreApplication>
#include <QFile>
#include <QJsonDocument>
#include <QJsonValue>
#include <QTextStream>

namespace {
QString stringFromAnyKey(const QJsonObject &object, std::initializer_list<const char *> keys)
{
    for (const char *key : keys) {
        const QJsonValue value = object.value(QString::fromLatin1(key));
        if (value.isString()) {
            return value.toString();
        }
    }
    return {};
}

QString nestedString(const QJsonObject &object, const QString &parent, const QString &child)
{
    const QJsonValue parentValue = object.value(parent);
    if (!parentValue.isObject()) {
        return {};
    }
    const QJsonValue childValue = parentValue.toObject().value(child);
    return childValue.isString() ? childValue.toString() : QString();
}

QString payloadTool(const QJsonObject &payload)
{
    return stringFromAnyKey(payload, {"tool", "tool_name", "toolName"});
}

QString payloadCommand(const QJsonObject &payload)
{
    QString command = stringFromAnyKey(payload, {"command", "cmd"});
    if (command.isEmpty()) {
        command = nestedString(payload, QStringLiteral("tool_input"), QStringLiteral("command"));
    }
    if (command.isEmpty()) {
        command = nestedString(payload, QStringLiteral("input"), QStringLiteral("command"));
    }
    return command;
}

QString payloadError(const QJsonObject &payload)
{
    return stringFromAnyKey(payload, {"error", "error_message"});
}

bool isToolEvent(const QString &event)
{
    return event == QStringLiteral("PreToolUse") || event == QStringLiteral("PostToolUse");
}

QString normalizeEvent(QString event)
{
    event = event.trimmed();
    if (event.compare(QStringLiteral("permission_request"), Qt::CaseInsensitive) == 0) {
        return QStringLiteral("PermissionRequest");
    }
    if (event.compare(QStringLiteral("stop"), Qt::CaseInsensitive) == 0) {
        return QStringLiteral("Stop");
    }
    if (event.compare(QStringLiteral("pre_tool_use"), Qt::CaseInsensitive) == 0) {
        return QStringLiteral("PreToolUse");
    }
    if (event.compare(QStringLiteral("post_tool_use"), Qt::CaseInsensitive) == 0) {
        return QStringLiteral("PostToolUse");
    }
    return event;
}
}

int HookHandler::run(const QStringList &arguments)
{
    const QString input = readStdin();
    const QJsonObject payload = parsePayload(input);
    const QString event = eventName(payload, arguments);
    appendLog(QStringLiteral("hook event=%1 payload=%2").arg(event, input.left(4000)));

    const AppSettings settings = AppSettings::load();
    if (!shouldNotify(settings, event, payload)) {
        return 0;
    }

    const QString title = notificationTitle(event, payload);
    const QString message = buildMessage(event, payload);
    QString error;
    WxPushClient client;
    if (!client.sendMessage(settings, title, event, message, contextObject(payload), &error)) {
        appendLog(QStringLiteral("push failed: %1").arg(error));
        QTextStream(stderr) << error << Qt::endl;
        return 2;
    }

    appendLog(QStringLiteral("push sent: %1").arg(event));
    return 0;
}

QString HookHandler::readStdin() const
{
    QFile stdinFile;
    stdinFile.open(stdin, QIODevice::ReadOnly | QIODevice::Text);
    return QString::fromUtf8(stdinFile.readAll());
}

QJsonObject HookHandler::parsePayload(const QString &text) const
{
    const QJsonDocument doc = QJsonDocument::fromJson(text.toUtf8());
    if (doc.isObject()) {
        return doc.object();
    }
    return {};
}

QString HookHandler::eventName(const QJsonObject &payload, const QStringList &arguments) const
{
    const int eventArg = arguments.indexOf(QStringLiteral("--event"));
    if (eventArg >= 0 && eventArg + 1 < arguments.size()) {
        return normalizeEvent(arguments.at(eventArg + 1));
    }

    QString event = stringFromAnyKey(payload, {"hook_event_name", "hookEventName", "event", "event_name", "name"});
    if (event.isEmpty()) {
        event = nestedString(payload, QStringLiteral("hook"), QStringLiteral("event"));
    }
    if (event.isEmpty()) {
        event = qEnvironmentVariable("CODEX_HOOK_EVENT");
    }
    return normalizeEvent(event.isEmpty() ? QStringLiteral("Unknown") : event);
}

QString HookHandler::notificationTitle(const QString &event, const QJsonObject &payload) const
{
    if (!payloadError(payload).isEmpty()) {
        return QStringLiteral("Codex 执行报错");
    }
    if (event == QStringLiteral("PermissionRequest")) {
        return QStringLiteral("Codex 请求授权");
    }
    if (event == QStringLiteral("Stop")) {
        return QStringLiteral("Codex 任务完成");
    }
    if (!payloadCommand(payload).isEmpty()) {
        return QStringLiteral("Codex 执行命令");
    }
    if (isToolEvent(event) || !payloadTool(payload).isEmpty()) {
        return QStringLiteral("Codex 工具调用");
    }
    return QStringLiteral("Codex 任务状态");
}

QString HookHandler::buildMessage(const QString &event, const QJsonObject &payload) const
{
    const QString command = payloadCommand(payload);
    const QString tool = payloadTool(payload);
    const QString cwd = stringFromAnyKey(payload, {"cwd", "workspace", "project_dir"});
    const QString error = stringFromAnyKey(payload, {"error", "error_message", "message"});

    QStringList lines;
    if (!payloadError(payload).isEmpty()) {
        lines << QStringLiteral("Codex 执行过程中检测到报错。");
    } else if (event == QStringLiteral("PermissionRequest")) {
        lines << QStringLiteral("Codex 正在等待权限确认。");
    } else if (event == QStringLiteral("Stop")) {
        lines << QStringLiteral("Codex 本轮任务已停止或完成。");
    } else if (!command.isEmpty()) {
        lines << QStringLiteral("Codex 正在执行命令。");
    } else if (isToolEvent(event) || !tool.isEmpty()) {
        lines << QStringLiteral("Codex 正在调用工具。");
    } else {
        lines << QStringLiteral("Codex 触发事件: %1").arg(event);
    }

    if (!tool.isEmpty()) {
        lines << QStringLiteral("工具: %1").arg(tool);
    }
    if (!command.isEmpty()) {
        lines << QStringLiteral("命令: %1").arg(command.left(500));
    }
    if (!cwd.isEmpty()) {
        lines << QStringLiteral("目录: %1").arg(cwd);
    }
    if (!error.isEmpty()) {
        lines << QStringLiteral("信息: %1").arg(error.left(500));
    }
    return lines.join(QLatin1Char('\n'));
}

QJsonObject HookHandler::contextObject(const QJsonObject &payload) const
{
    QJsonObject context;
    const QString workspace = stringFromAnyKey(payload, {"workspace", "cwd", "project_dir"});
    const QString tool = payloadTool(payload);
    const QString command = payloadCommand(payload);
    context.insert(QStringLiteral("workspace"), workspace);
    context.insert(QStringLiteral("cwd"), workspace);
    context.insert(QStringLiteral("tool"), tool);
    context.insert(QStringLiteral("command"), command.left(500));
    return context;
}

bool HookHandler::shouldNotify(const AppSettings &settings, const QString &event, const QJsonObject &payload) const
{
    if (!settings.enabled || !settings.isPushConfigured()) {
        return false;
    }
    const QString error = payloadError(payload);
    const QString command = payloadCommand(payload);
    const QString tool = payloadTool(payload);

    if (!error.isEmpty()) {
        return settings.notifyErrors;
    }
    if (event == QStringLiteral("PermissionRequest")) {
        return settings.notifyPermissionRequest;
    }
    if (event == QStringLiteral("Stop")) {
        return settings.notifyStop;
    }
    if (!command.isEmpty()) {
        return settings.notifyCommands;
    }
    if (isToolEvent(event) || !tool.isEmpty()) {
        return settings.notifyToolUse;
    }

    return false;
}
