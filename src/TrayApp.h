#pragma once

#include <QObject>

class QAction;
class MainWindow;
class QSystemTrayIcon;

class TrayApp : public QObject
{
    Q_OBJECT

public:
    explicit TrayApp(QObject *parent = nullptr);

private:
    void showSettings();
    void installHooks();
    void uninstallHooks();
    void testPush();
    void openLog();
    void setEnabled(bool enabled);
    void refreshMenuState();
    QIcon trayIcon() const;

    QSystemTrayIcon *tray_ = nullptr;
    QAction *enabledAction_ = nullptr;
    MainWindow *mainWindow_ = nullptr;
};
