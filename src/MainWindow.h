#pragma once

#include "AppSettings.h"

#include <QMainWindow>

class QCheckBox;
class QLabel;
class QLineEdit;
class QPlainTextEdit;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);

signals:
    void settingsChanged();

private:
    QWidget *section(const QString &title, const QString &subtitle = {});
    QWidget *statusItem(const QString &title, const QString &value, bool ok) const;

    void applyStyle();
    void loadSettings();
    AppSettings settingsFromForm() const;
    bool validateTemplateJson() const;
    void saveSettings();
    void saveNotificationOptions();
    void refreshStatus();
    void refreshLog();
    void updateDetailUrlPreview();
    void updateStartupState();
    void installHooks();
    void uninstallHooks();
    void testPush();
    void openLog();

    QLabel *enabledStatus_ = nullptr;
    QLabel *configStatus_ = nullptr;
    QLabel *hookStatus_ = nullptr;
    QLabel *detailStatus_ = nullptr;

    QCheckBox *enabledCheck_ = nullptr;
    QCheckBox *startupCheck_ = nullptr;
    QCheckBox *permissionCheck_ = nullptr;
    QCheckBox *stopCheck_ = nullptr;
    QCheckBox *errorsCheck_ = nullptr;
    QCheckBox *toolUseCheck_ = nullptr;
    QCheckBox *commandsCheck_ = nullptr;
    QLineEdit *appIdEdit_ = nullptr;
    QLineEdit *secretEdit_ = nullptr;
    QLineEdit *userIdEdit_ = nullptr;
    QLineEdit *templateIdEdit_ = nullptr;
    QLineEdit *apiBaseEdit_ = nullptr;
    QLineEdit *messageUrlEdit_ = nullptr;
    QLineEdit *detailUrlPreviewEdit_ = nullptr;
    QPlainTextEdit *templateDataEdit_ = nullptr;
    QPlainTextEdit *logEdit_ = nullptr;
};
