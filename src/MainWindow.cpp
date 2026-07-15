#include "MainWindow.h"

#include "HookInstaller.h"
#include "WxPushClient.h"

#include <QCheckBox>
#include <QCoreApplication>
#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFormLayout>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QJsonDocument>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QSettings>
#include <QUrl>
#include <QUrlQuery>
#include <QVBoxLayout>

namespace {
QLabel *mutedLabel(const QString &text)
{
    auto *label = new QLabel(text);
    label->setObjectName(QStringLiteral("muted"));
    label->setWordWrap(true);
    return label;
}

QString readRecentLog(int maxLines = 80)
{
    QFile file(AppSettings::logPath());
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return {};
    }

    QStringList lines = QString::fromUtf8(file.readAll()).split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    if (lines.size() > maxLines) {
        lines = lines.mid(lines.size() - maxLines);
    }
    return lines.join(QLatin1Char('\n'));
}

QString startupRegistryValue()
{
    return QStringLiteral("\"%1\"").arg(QDir::toNativeSeparators(QCoreApplication::applicationFilePath()));
}

bool isStartupEnabled()
{
#ifdef Q_OS_WIN
    QSettings registry(QStringLiteral("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run"),
                       QSettings::NativeFormat);
    return registry.value(QStringLiteral("codex-wxpush")).toString() == startupRegistryValue();
#else
    return false;
#endif
}

bool setStartupEnabled(bool enabled)
{
#ifdef Q_OS_WIN
    QSettings registry(QStringLiteral("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run"),
                       QSettings::NativeFormat);
    if (enabled) {
        registry.setValue(QStringLiteral("codex-wxpush"), startupRegistryValue());
    } else {
        registry.remove(QStringLiteral("codex-wxpush"));
    }
    registry.sync();
    return registry.status() == QSettings::NoError;
#else
    Q_UNUSED(enabled);
    return true;
#endif
}
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle(QStringLiteral("codex-wxpush"));
    resize(900, 720);
    setMinimumSize(780, 560);

    auto *scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);

    auto *page = new QWidget;
    auto *root = new QVBoxLayout(page);
    root->setContentsMargins(24, 24, 24, 24);
    root->setSpacing(16);

    auto *header = new QWidget;
    auto *headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(0, 0, 0, 0);
    headerLayout->setSpacing(16);
    auto *titleBox = new QWidget;
    auto *titleLayout = new QVBoxLayout(titleBox);
    titleLayout->setContentsMargins(0, 0, 0, 0);
    titleLayout->setSpacing(4);
    auto *title = new QLabel(QStringLiteral("codex-wxpush"));
    title->setObjectName(QStringLiteral("title"));
    titleLayout->addWidget(title);
    titleLayout->addWidget(mutedLabel(QStringLiteral("Codex Hook 到微信模板消息的最小配置工具")));
    headerLayout->addWidget(titleBox, 1);

    auto *saveButton = new QPushButton(QStringLiteral("保存"));
    saveButton->setObjectName(QStringLiteral("primaryButton"));
    auto *testButton = new QPushButton(QStringLiteral("测试推送"));
    auto *installButton = new QPushButton(QStringLiteral("安装 Hook"));
    auto *uninstallButton = new QPushButton(QStringLiteral("卸载 Hook"));
    connect(saveButton, &QPushButton::clicked, this, &MainWindow::saveSettings);
    connect(testButton, &QPushButton::clicked, this, &MainWindow::testPush);
    connect(installButton, &QPushButton::clicked, this, &MainWindow::installHooks);
    connect(uninstallButton, &QPushButton::clicked, this, &MainWindow::uninstallHooks);
    headerLayout->addWidget(saveButton);
    headerLayout->addWidget(testButton);
    headerLayout->addWidget(installButton);
    headerLayout->addWidget(uninstallButton);
    root->addWidget(header);

    auto *status = section(QStringLiteral("状态"));
    auto *statusLayout = qobject_cast<QVBoxLayout *>(status->layout());
    auto *statusGrid = new QGridLayout;
    statusGrid->setSpacing(10);
    enabledStatus_ = new QLabel;
    configStatus_ = new QLabel;
    hookStatus_ = new QLabel;
    detailStatus_ = new QLabel;
    statusGrid->addWidget(statusItem(QStringLiteral("推送"), QStringLiteral(""), true), 0, 0);
    statusGrid->addWidget(statusItem(QStringLiteral("微信配置"), QStringLiteral(""), true), 0, 1);
    statusGrid->addWidget(statusItem(QStringLiteral("Hook"), QStringLiteral(""), true), 1, 0);
    statusGrid->addWidget(statusItem(QStringLiteral("详情页"), QStringLiteral(""), true), 1, 1);
    statusLayout->addLayout(statusGrid);
    root->addWidget(status);

    auto *config = section(QStringLiteral("配置"), QStringLiteral("选择推送条件，并填写微信模板消息所需字段。"));
    auto *configLayout = qobject_cast<QVBoxLayout *>(config->layout());
    auto *form = new QFormLayout;
    form->setLabelAlignment(Qt::AlignLeft);
    form->setHorizontalSpacing(16);
    form->setVerticalSpacing(10);

    enabledCheck_ = new QCheckBox(QStringLiteral("启用推送"));
    startupCheck_ = new QCheckBox(QStringLiteral("开机启动"));
    permissionCheck_ = new QCheckBox(QStringLiteral("授权"));
    stopCheck_ = new QCheckBox(QStringLiteral("任务完成"));
    errorsCheck_ = new QCheckBox(QStringLiteral("报错"));
    toolUseCheck_ = new QCheckBox(QStringLiteral("工具调用"));
    commandsCheck_ = new QCheckBox(QStringLiteral("执行命令"));
    auto *checks = new QWidget;
    auto *checksLayout = new QHBoxLayout(checks);
    checksLayout->setContentsMargins(0, 0, 0, 0);
    checksLayout->setSpacing(14);
    checksLayout->addWidget(enabledCheck_);
    checksLayout->addWidget(startupCheck_);
    checksLayout->addWidget(permissionCheck_);
    checksLayout->addWidget(stopCheck_);
    checksLayout->addWidget(errorsCheck_);
    checksLayout->addWidget(toolUseCheck_);
    checksLayout->addWidget(commandsCheck_);
    checksLayout->addStretch(1);
    form->addRow(QStringLiteral("推送条件"), checks);

    appIdEdit_ = new QLineEdit;
    secretEdit_ = new QLineEdit;
    secretEdit_->setEchoMode(QLineEdit::Password);
    userIdEdit_ = new QLineEdit;
    templateIdEdit_ = new QLineEdit;
    apiBaseEdit_ = new QLineEdit;
    messageUrlEdit_ = new QLineEdit;
    detailUrlPreviewEdit_ = new QLineEdit;
    detailUrlPreviewEdit_->setReadOnly(true);

    form->addRow(QStringLiteral("AppID"), appIdEdit_);
    form->addRow(QStringLiteral("Secret"), secretEdit_);
    form->addRow(QStringLiteral("OpenID"), userIdEdit_);
    form->addRow(QStringLiteral("模板 ID"), templateIdEdit_);
    form->addRow(QStringLiteral("API 地址"), apiBaseEdit_);
    form->addRow(QStringLiteral("详情 URL"), messageUrlEdit_);
    form->addRow(QStringLiteral("详情预览"), detailUrlPreviewEdit_);
    configLayout->addLayout(form);
    root->addWidget(config);

    auto *templateSection = section(QStringLiteral("模板 data JSON"), QStringLiteral("字段名必须和微信测试号模板匹配。"));
    auto *templateLayout = qobject_cast<QVBoxLayout *>(templateSection->layout());
    templateDataEdit_ = new QPlainTextEdit;
    templateDataEdit_->setObjectName(QStringLiteral("codeEdit"));
    templateDataEdit_->setMinimumHeight(190);
    templateLayout->addWidget(templateDataEdit_);
    root->addWidget(templateSection);

    auto *logSection = section(QStringLiteral("最近日志"));
    auto *logLayout = qobject_cast<QVBoxLayout *>(logSection->layout());
    logEdit_ = new QPlainTextEdit;
    logEdit_->setObjectName(QStringLiteral("codeEdit"));
    logEdit_->setReadOnly(true);
    logEdit_->setMinimumHeight(160);
    logLayout->addWidget(logEdit_);
    auto *logActions = new QHBoxLayout;
    logActions->addStretch(1);
    auto *refreshLogButton = new QPushButton(QStringLiteral("刷新日志"));
    auto *openLogButton = new QPushButton(QStringLiteral("打开日志文件"));
    connect(refreshLogButton, &QPushButton::clicked, this, &MainWindow::refreshLog);
    connect(openLogButton, &QPushButton::clicked, this, &MainWindow::openLog);
    logActions->addWidget(refreshLogButton);
    logActions->addWidget(openLogButton);
    logLayout->addLayout(logActions);
    root->addWidget(logSection);

    scroll->setWidget(page);
    setCentralWidget(scroll);

    connect(messageUrlEdit_, &QLineEdit::textChanged, this, &MainWindow::updateDetailUrlPreview);

    applyStyle();
    loadSettings();
    refreshStatus();
    refreshLog();
}

QWidget *MainWindow::section(const QString &title, const QString &subtitle)
{
    auto *frame = new QFrame;
    frame->setObjectName(QStringLiteral("section"));
    auto *layout = new QVBoxLayout(frame);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(10);

    auto *titleLabel = new QLabel(title);
    titleLabel->setObjectName(QStringLiteral("sectionTitle"));
    layout->addWidget(titleLabel);
    if (!subtitle.isEmpty()) {
        layout->addWidget(mutedLabel(subtitle));
    }
    return frame;
}

QWidget *MainWindow::statusItem(const QString &title, const QString &, bool) const
{
    auto *frame = new QFrame;
    frame->setObjectName(QStringLiteral("statusItem"));
    auto *layout = new QVBoxLayout(frame);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(5);
    auto *titleLabel = mutedLabel(title);
    auto *valueLabel = new QLabel(QStringLiteral("-"));
    valueLabel->setObjectName(QStringLiteral("statusValue"));
    if (title == QStringLiteral("推送")) {
        const_cast<MainWindow *>(this)->enabledStatus_ = valueLabel;
    } else if (title == QStringLiteral("微信配置")) {
        const_cast<MainWindow *>(this)->configStatus_ = valueLabel;
    } else if (title == QStringLiteral("Hook")) {
        const_cast<MainWindow *>(this)->hookStatus_ = valueLabel;
    } else if (title == QStringLiteral("详情页")) {
        const_cast<MainWindow *>(this)->detailStatus_ = valueLabel;
    }
    layout->addWidget(titleLabel);
    layout->addWidget(valueLabel);
    return frame;
}

void MainWindow::applyStyle()
{
    setStyleSheet(QStringLiteral(R"(
        QMainWindow, QScrollArea { background: #fbfaf7; }
        QLabel#title { color: #25221f; font-size: 22px; font-weight: 760; }
        QLabel#sectionTitle { color: #25221f; font-size: 15px; font-weight: 760; }
        QLabel#muted, QLabel[objectName="muted"] { color: #6f6961; font-size: 12px; }
        QLabel#statusValue { color: #25221f; font-size: 20px; font-weight: 760; }
        QFrame#section, QFrame#statusItem {
            background: #ffffff;
            border: 1px solid #ded8cf;
            border-radius: 8px;
        }
        QPushButton {
            min-height: 34px;
            border-radius: 8px;
            border: 1px solid #ded8cf;
            background: #ffffff;
            color: #25221f;
            padding: 0 12px;
        }
        QPushButton:hover { background: #f4f1ec; }
        QPushButton#primaryButton { background: #25221f; color: #ffffff; border-color: #25221f; font-weight: 720; }
        QLineEdit, QPlainTextEdit {
            background: #ffffff;
            border: 1px solid #ded8cf;
            border-radius: 8px;
            padding: 8px 10px;
            color: #25221f;
        }
        QPlainTextEdit#codeEdit { font-family: Consolas, "Cascadia Mono", monospace; font-size: 13px; }
        QCheckBox { color: #25221f; spacing: 7px; }
    )"));
}

void MainWindow::loadSettings()
{
    const AppSettings settings = AppSettings::load();
    enabledCheck_->setChecked(settings.enabled);
    startupCheck_->setChecked(isStartupEnabled());
    permissionCheck_->setChecked(settings.notifyPermissionRequest);
    stopCheck_->setChecked(settings.notifyStop);
    errorsCheck_->setChecked(settings.notifyErrors);
    toolUseCheck_->setChecked(settings.notifyToolUse);
    commandsCheck_->setChecked(settings.notifyCommands);
    appIdEdit_->setText(settings.appId);
    secretEdit_->setText(settings.appSecret);
    userIdEdit_->setText(settings.userId);
    templateIdEdit_->setText(settings.templateId);
    apiBaseEdit_->setText(settings.apiBaseUrl);
    messageUrlEdit_->setText(settings.messageUrl);
    templateDataEdit_->setPlainText(settings.templateDataJson);
    updateDetailUrlPreview();
}

AppSettings MainWindow::settingsFromForm() const
{
    AppSettings settings;
    settings.enabled = enabledCheck_->isChecked();
    settings.notifyPermissionRequest = permissionCheck_->isChecked();
    settings.notifyStop = stopCheck_->isChecked();
    settings.notifyErrors = errorsCheck_->isChecked();
    settings.notifyToolUse = toolUseCheck_->isChecked();
    settings.notifyCommands = commandsCheck_->isChecked();
    settings.appId = appIdEdit_->text().trimmed();
    settings.appSecret = secretEdit_->text().trimmed();
    settings.userId = userIdEdit_->text().trimmed();
    settings.templateId = templateIdEdit_->text().trimmed();
    settings.apiBaseUrl = apiBaseEdit_->text().trimmed();
    settings.messageUrl = messageUrlEdit_->text().trimmed();
    settings.templateDataJson = templateDataEdit_->toPlainText();
    return settings;
}

bool MainWindow::validateTemplateJson() const
{
    QJsonParseError error;
    const QJsonDocument doc = QJsonDocument::fromJson(templateDataEdit_->toPlainText().toUtf8(), &error);
    if (error.error != QJsonParseError::NoError || !doc.isObject()) {
        QMessageBox::warning(const_cast<MainWindow *>(this),
                             QStringLiteral("模板 JSON 无效"),
                             QStringLiteral("模板 data 必须是 JSON 对象: %1").arg(error.errorString()));
        return false;
    }
    return true;
}

void MainWindow::saveSettings()
{
    if (!validateTemplateJson()) {
        return;
    }

    AppSettings settings = settingsFromForm();
    settings.save();
    if (!setStartupEnabled(startupCheck_->isChecked())) {
        QMessageBox::warning(this, QStringLiteral("开机启动设置失败"), QStringLiteral("无法写入当前用户的开机启动项。"));
        return;
    }
    appendLog(QStringLiteral("settings saved"));
    emit settingsChanged();
    refreshStatus();
    QMessageBox::information(this, QStringLiteral("已保存"), QStringLiteral("配置已保存。"));
}

void MainWindow::refreshStatus()
{
    const AppSettings settings = settingsFromForm();
    enabledStatus_->setText(settings.enabled ? QStringLiteral("启用") : QStringLiteral("暂停"));
    configStatus_->setText(settings.isPushConfigured() ? QStringLiteral("完整") : QStringLiteral("缺少字段"));
    hookStatus_->setText(QFile::exists(HookInstaller::hooksPath()) ? QStringLiteral("已安装") : QStringLiteral("未安装"));
    detailStatus_->setText(settings.messageUrl.trimmed().isEmpty() ? QStringLiteral("未设置") : QStringLiteral("已设置"));
    startupCheck_->blockSignals(true);
    startupCheck_->setChecked(isStartupEnabled());
    startupCheck_->blockSignals(false);
}

void MainWindow::refreshLog()
{
    const QString text = readRecentLog();
    logEdit_->setPlainText(text.isEmpty() ? QStringLiteral("暂无日志。") : text);
}

void MainWindow::updateDetailUrlPreview()
{
    QUrl url(messageUrlEdit_->text().trimmed());
    if (url.isEmpty() || !url.isValid()) {
        detailUrlPreviewEdit_->clear();
        refreshStatus();
        return;
    }

    QUrlQuery query(url);
    query.removeAllQueryItems(QStringLiteral("title"));
    query.removeAllQueryItems(QStringLiteral("message"));
    query.removeAllQueryItems(QStringLiteral("date"));
    query.addQueryItem(QStringLiteral("title"), QStringLiteral("Codex 任务状态"));
    query.addQueryItem(QStringLiteral("message"), QStringLiteral("Codex 本轮任务已停止或完成。"));
    query.addQueryItem(QStringLiteral("date"), QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")));
    url.setQuery(query);
    detailUrlPreviewEdit_->setText(url.toString(QUrl::FullyEncoded));
    detailUrlPreviewEdit_->setCursorPosition(0);
    refreshStatus();
}

void MainWindow::installHooks()
{
    QString error;
    if (!HookInstaller::install(&error)) {
        QMessageBox::warning(this, QStringLiteral("安装失败"), error);
        return;
    }
    refreshStatus();
    refreshLog();
    QMessageBox::information(this, QStringLiteral("安装完成"), QStringLiteral("Codex hooks 已写入。"));
}

void MainWindow::uninstallHooks()
{
    QString error;
    if (!HookInstaller::uninstall(&error)) {
        QMessageBox::warning(this, QStringLiteral("卸载失败"), error);
        return;
    }
    refreshStatus();
    refreshLog();
    QMessageBox::information(this, QStringLiteral("卸载完成"), QStringLiteral("Codex hooks 已移除。"));
}

void MainWindow::testPush()
{
    if (!validateTemplateJson()) {
        return;
    }

    const AppSettings settings = settingsFromForm();
    QString error;
    WxPushClient client(this);
    const bool ok = client.sendMessage(settings,
                                       QStringLiteral("Codex 微信提醒测试"),
                                       QStringLiteral("Test"),
                                       QStringLiteral("这是一条来自 codex-wxpush 的测试消息。"),
                                       QJsonObject{{QStringLiteral("workspace"), QDir::currentPath()}},
                                       &error);
    if (!ok) {
        QMessageBox::warning(this, QStringLiteral("测试失败"), error);
        return;
    }

    appendLog(QStringLiteral("manual test push sent"));
    refreshStatus();
    refreshLog();
    QMessageBox::information(this, QStringLiteral("测试成功"), QStringLiteral("测试消息已发送。"));
}

void MainWindow::openLog()
{
    QFile file(AppSettings::logPath());
    if (!file.exists()) {
        appendLog(QStringLiteral("log file created"));
    }
    QDesktopServices::openUrl(QUrl::fromLocalFile(AppSettings::logPath()));
}
