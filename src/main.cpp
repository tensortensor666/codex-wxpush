#include "AppSettings.h"
#include "HookHandler.h"
#include "HookInstaller.h"
#include "MainWindow.h"
#include "TrayApp.h"
#include "WxPushClient.h"

#include <QApplication>
#include <QCoreApplication>
#include <QDir>
#include <QJsonObject>
#include <QTextStream>

namespace {
bool hasArg(int argc, char *argv[], const QString &name)
{
    for (int i = 1; i < argc; ++i) {
        if (QString::fromLocal8Bit(argv[i]) == name) {
            return true;
        }
    }
    return false;
}

QString optionValue(const QStringList &args, const QString &name)
{
    const int index = args.indexOf(name);
    if (index >= 0 && index + 1 < args.size()) {
        return args.at(index + 1);
    }
    return {};
}

int runSendCommand(const QStringList &args)
{
    AppSettings settings = AppSettings::load();
    settings.enabled = true;

    const QString appId = optionValue(args, QStringLiteral("-appid"));
    const QString secret = optionValue(args, QStringLiteral("-secret"));
    const QString userId = optionValue(args, QStringLiteral("-userid"));
    const QString templateId = optionValue(args, QStringLiteral("-template_id"));
    const QString apiBase = optionValue(args, QStringLiteral("-api_base"));
    const QString url = optionValue(args, QStringLiteral("-url"));

    if (!appId.isEmpty()) {
        settings.appId = appId;
    }
    if (!secret.isEmpty()) {
        settings.appSecret = secret;
    }
    if (!userId.isEmpty()) {
        settings.userId = userId;
    }
    if (!templateId.isEmpty()) {
        settings.templateId = templateId;
    }
    if (!apiBase.isEmpty()) {
        settings.apiBaseUrl = apiBase;
    }
    if (!url.isEmpty()) {
        settings.messageUrl = url;
    }

    const QString title = optionValue(args, QStringLiteral("-title")).isEmpty()
        ? QStringLiteral("Codex 微信提醒测试")
        : optionValue(args, QStringLiteral("-title"));
    const QString content = optionValue(args, QStringLiteral("-content")).isEmpty()
        ? QStringLiteral("这是一条测试消息。")
        : optionValue(args, QStringLiteral("-content"));

    QString error;
    WxPushClient client;
    const bool ok = client.sendMessage(settings,
                                       title,
                                       QStringLiteral("ManualTest"),
                                       content,
                                       QJsonObject{{QStringLiteral("workspace"), QDir::currentPath()}},
                                       &error);
    QTextStream out(stdout);
    QTextStream err(stderr);
    if (!ok) {
        err << error << Qt::endl;
        return 2;
    }
    out << "sent" << Qt::endl;
    return 0;
}
}

int main(int argc, char *argv[])
{
    const bool hookMode = hasArg(argc, argv, QStringLiteral("--hook"));
    const bool windowMode = hasArg(argc, argv, QStringLiteral("--window"));
    const bool installHooksMode = hasArg(argc, argv, QStringLiteral("--install-hooks"));
    const bool uninstallHooksMode = hasArg(argc, argv, QStringLiteral("--uninstall-hooks"));
    const bool sendMode = hasArg(argc, argv, QStringLiteral("--send")) || hasArg(argc, argv, QStringLiteral("-title"));

    if (hookMode) {
        QCoreApplication app(argc, argv);
        QCoreApplication::setOrganizationName(QStringLiteral("codex-wxpush"));
        QCoreApplication::setApplicationName(QStringLiteral("codex-wxpush"));
        HookHandler handler;
        return handler.run(QCoreApplication::arguments());
    }

    if (sendMode) {
        QCoreApplication app(argc, argv);
        QCoreApplication::setOrganizationName(QStringLiteral("codex-wxpush"));
        QCoreApplication::setApplicationName(QStringLiteral("codex-wxpush"));
        return runSendCommand(QCoreApplication::arguments());
    }

    if (installHooksMode || uninstallHooksMode) {
        QCoreApplication app(argc, argv);
        QCoreApplication::setOrganizationName(QStringLiteral("codex-wxpush"));
        QCoreApplication::setApplicationName(QStringLiteral("codex-wxpush"));
        QString error;
        const bool ok = installHooksMode ? HookInstaller::install(&error) : HookInstaller::uninstall(&error);
        if (!ok) {
            QTextStream(stderr) << error << Qt::endl;
            return 2;
        }
        QTextStream(stdout) << (installHooksMode ? "installed" : "uninstalled") << Qt::endl;
        return 0;
    }

    if (windowMode) {
        QApplication app(argc, argv);
        QCoreApplication::setOrganizationName(QStringLiteral("codex-wxpush"));
        QCoreApplication::setApplicationName(QStringLiteral("codex-wxpush"));
        MainWindow window;
        window.show();
        return app.exec();
    }

    QApplication app(argc, argv);
    QApplication::setQuitOnLastWindowClosed(false);
    QCoreApplication::setOrganizationName(QStringLiteral("codex-wxpush"));
    QCoreApplication::setApplicationName(QStringLiteral("codex-wxpush"));

    TrayApp tray;
    return app.exec();
}
