#include "TrayApp.h"

#include "AppSettings.h"
#include "HookInstaller.h"
#include "MainWindow.h"
#include "WxPushClient.h"

#include <QAction>
#include <QApplication>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QIcon>
#include <QMenu>
#include <QMessageBox>
#include <QPainter>
#include <QPixmap>
#include <QSystemTrayIcon>
#include <QUrl>

TrayApp::TrayApp(QObject *parent)
    : QObject(parent)
{
    tray_ = new QSystemTrayIcon(trayIcon(), this);
    tray_->setToolTip(QStringLiteral("Codex 微信提醒"));

    auto *menu = new QMenu;
    enabledAction_ = menu->addAction(QStringLiteral("启用推送"));
    enabledAction_->setCheckable(true);
    connect(enabledAction_, &QAction::toggled, this, &TrayApp::setEnabled);

    menu->addSeparator();
    QAction *settingsAction = menu->addAction(QStringLiteral("打开控制台"));
    QAction *testAction = menu->addAction(QStringLiteral("测试推送"));
    QAction *installAction = menu->addAction(QStringLiteral("安装 Codex Hook"));
    QAction *uninstallAction = menu->addAction(QStringLiteral("卸载 Codex Hook"));
    QAction *logAction = menu->addAction(QStringLiteral("查看日志"));
    menu->addSeparator();
    QAction *quitAction = menu->addAction(QStringLiteral("退出"));

    connect(settingsAction, &QAction::triggered, this, &TrayApp::showSettings);
    connect(testAction, &QAction::triggered, this, &TrayApp::testPush);
    connect(installAction, &QAction::triggered, this, &TrayApp::installHooks);
    connect(uninstallAction, &QAction::triggered, this, &TrayApp::uninstallHooks);
    connect(logAction, &QAction::triggered, this, &TrayApp::openLog);
    connect(quitAction, &QAction::triggered, qApp, &QApplication::quit);
    connect(tray_, &QSystemTrayIcon::activated, this, [this](QSystemTrayIcon::ActivationReason reason) {
        if (reason == QSystemTrayIcon::DoubleClick || reason == QSystemTrayIcon::Trigger) {
            showSettings();
        }
    });

    tray_->setContextMenu(menu);
    refreshMenuState();
    tray_->show();
    tray_->showMessage(QStringLiteral("Codex 微信提醒"),
                       QStringLiteral("托盘程序已启动。"),
                       QSystemTrayIcon::Information,
                       2500);
}

void TrayApp::showSettings()
{
    if (!mainWindow_) {
        mainWindow_ = new MainWindow;
        connect(mainWindow_, &MainWindow::settingsChanged, this, &TrayApp::refreshMenuState);
    }
    mainWindow_->show();
    mainWindow_->raise();
    mainWindow_->activateWindow();
}

void TrayApp::installHooks()
{
    QString error;
    if (!HookInstaller::install(&error)) {
        QMessageBox::warning(nullptr, QStringLiteral("安装失败"), error);
        return;
    }
    QMessageBox::information(nullptr,
                             QStringLiteral("安装完成"),
                             QStringLiteral("Codex hooks 已写入:\n%1\n\n首次运行可能需要在 Codex 中信任该 hook。").arg(HookInstaller::hooksPath()));
}

void TrayApp::uninstallHooks()
{
    QString error;
    if (!HookInstaller::uninstall(&error)) {
        QMessageBox::warning(nullptr, QStringLiteral("卸载失败"), error);
        return;
    }
    QMessageBox::information(nullptr, QStringLiteral("卸载完成"), QStringLiteral("Codex hooks 已移除。"));
}

void TrayApp::testPush()
{
    const AppSettings settings = AppSettings::load();
    QString error;
    WxPushClient client(this);
    const bool ok = client.sendMessage(settings,
                                       QStringLiteral("Codex 微信提醒测试"),
                                       QStringLiteral("Test"),
                                       QStringLiteral("这是一条来自托盘菜单的测试消息。"),
                                       QJsonObject{{QStringLiteral("workspace"), QDir::currentPath()}},
                                       &error);
    if (!ok) {
        QMessageBox::warning(nullptr, QStringLiteral("测试失败"), error);
        return;
    }
    tray_->showMessage(QStringLiteral("测试成功"), QStringLiteral("测试消息已发送。"), QSystemTrayIcon::Information, 2500);
}

void TrayApp::openLog()
{
    QFile file(AppSettings::logPath());
    if (!file.exists()) {
        appendLog(QStringLiteral("log file created"));
    }
    QDesktopServices::openUrl(QUrl::fromLocalFile(AppSettings::logPath()));
}

void TrayApp::setEnabled(bool enabled)
{
    AppSettings settings = AppSettings::load();
    if (settings.enabled == enabled) {
        return;
    }
    settings.enabled = enabled;
    settings.save();
}

void TrayApp::refreshMenuState()
{
    const AppSettings settings = AppSettings::load();
    enabledAction_->blockSignals(true);
    enabledAction_->setChecked(settings.enabled);
    enabledAction_->blockSignals(false);
}

QIcon TrayApp::trayIcon() const
{
    QPixmap pixmap(64, 64);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setBrush(QColor(24, 160, 88));
    painter.setPen(Qt::NoPen);
    painter.drawRoundedRect(QRectF(8, 10, 48, 38), 10, 10);
    painter.setBrush(Qt::white);
    painter.drawEllipse(QPointF(25, 29), 4, 4);
    painter.drawEllipse(QPointF(39, 29), 4, 4);
    painter.setBrush(QColor(24, 160, 88));
    painter.drawPolygon(QPolygonF{QPointF(22, 46), QPointF(30, 46), QPointF(20, 56)});
    return QIcon(pixmap);
}
