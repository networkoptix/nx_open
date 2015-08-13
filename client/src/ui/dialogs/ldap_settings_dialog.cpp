#include "ldap_settings_dialog.h"
#include "ui_ldap_settings_dialog.h"

#include <QtWidgets/QMessageBox>
#include <QtCore/QTimer>

#include <core/resource_management/resource_pool.h>
#include <core/resource/media_server_resource.h>
#include <api/app_server_connection.h>
#include <api/global_settings.h>
#include <api/model/test_ldap_settings_reply.h>
#include <utils/common/ldap.h>

#include <ui/style/warning_style.h>
#include <ui/help/help_topic_accessor.h>
#include <ui/help/help_topics.h>

namespace {
    const int testLdapTimeoutMSec = 20 * 1000;

    int defaultLdapPort(bool ssl = false) {
        return ssl ? 636 : 389;
    }
}

class QnLdapSettingsDialogPrivate: public QObject {
public:
    QnLdapSettingsDialogPrivate(QnLdapSettingsDialog *parent);

    QnLdapSettingsDialog *dialog;
    int testHandle;
    QTimer *timeoutTimer;
    QPushButton *testButton;

    void testSettings();
    void showTestResult(const QString &text);
    void stopTesting(const QString &text = QString());
    QnLdapSettings settings() const;
    void updateFromSettings();

    void at_timeoutTimer_timeout();
    void at_serverLineEdit_editingFinished();
};

QnLdapSettingsDialogPrivate::QnLdapSettingsDialogPrivate(QnLdapSettingsDialog *parent)
    : QObject(parent)
    , dialog(parent)
    , testHandle(-1)
    , timeoutTimer(new QTimer(parent))
{
    connect(QnGlobalSettings::instance(), &QnGlobalSettings::ldapSettingsChanged, this, &QnLdapSettingsDialogPrivate::updateFromSettings);
    connect(timeoutTimer, &QTimer::timeout, this, &QnLdapSettingsDialogPrivate::at_timeoutTimer_timeout);
}

void QnLdapSettingsDialogPrivate::testSettings() {
    QnLdapSettings settings = this->settings();

    if (!settings.isValid()) {
        stopTesting(tr("The provided settings are not valid.") + lit("\n") + tr("Could not perform a test."));
        return;
    }

    QnMediaServerConnectionPtr serverConnection;
    for (const QnMediaServerResourcePtr server: qnResPool->getAllServers()) {
        if (server->getStatus() != Qn::Online)
            continue;

        if (!(server->getServerFlags() & Qn::SF_HasPublicIP))
            continue;

        serverConnection = server->apiConnection();
        break;
    }

    if (!serverConnection) {
        stopTesting(tr("None of your servers is connected to the Internet.") + lit("\n") + tr("Could not perform a test."));
        return;
    }

    dialog->ui->fieldsWidget->setEnabled(false);
    testButton->setEnabled(false);
    dialog->ui->buttonBox->button(QDialogButtonBox::Ok)->hide();

    dialog->ui->testProgressBar->setValue(0);
    dialog->ui->testStackWidget->setCurrentWidget(dialog->ui->testProgressPage);

    timeoutTimer->setInterval(testLdapTimeoutMSec / dialog->ui->testProgressBar->maximum());
    timeoutTimer->start();

    testHandle = serverConnection->testLdapSettingsAsync(settings, dialog, SLOT(at_testLdapSettingsFinished(int, const QnTestLdapSettingsReply&, int)));
}

void QnLdapSettingsDialogPrivate::showTestResult(const QString &text) {
    dialog->ui->testResultLabel->setText(text);
    dialog->ui->testStackWidget->setCurrentWidget(dialog->ui->testResultPage);
}

void QnLdapSettingsDialogPrivate::stopTesting(const QString &text) {
    if (testHandle < 0)
        return;

    timeoutTimer->stop();
    testHandle = -1;

    dialog->ui->buttonBox->button(QDialogButtonBox::Ok)->show();
    testButton->setEnabled(true);
    dialog->ui->fieldsWidget->setEnabled(true);
    showTestResult(text);
}

QnLdapSettings QnLdapSettingsDialogPrivate::settings() const {
    QnLdapSettings result;

    QUrl url = QUrl::fromUserInput(dialog->ui->serverLineEdit->text());
    if (!url.isValid())
        return result;

    if (url.port() == -1)
        url.setPort(defaultLdapPort(url.scheme() == lit("ldaps")));

    result.uri = url;
    result.adminDn = dialog->ui->adminDnLineEdit->text();
    result.adminPassword = dialog->ui->passwordLineEdit->text();
    result.searchBase = dialog->ui->searchBaseLineEdit->text();
    result.searchFilter = dialog->ui->searchFilterLineEdit->text();
    return result;
}

void QnLdapSettingsDialogPrivate::updateFromSettings() {
    stopTesting();

    const QnLdapSettings &settings = QnGlobalSettings::instance()->ldapSettings();

    QUrl url = settings.uri;
    if (url.port() == defaultLdapPort(url.scheme() == lit("ldaps")))
        url.setPort(-1);
    dialog->ui->serverLineEdit->setText(url.toString());
    dialog->ui->adminDnLineEdit->setText(settings.adminDn);
    dialog->ui->passwordLineEdit->setText(settings.adminPassword);
    dialog->ui->searchBaseLineEdit->setText(settings.searchBase);
    dialog->ui->searchFilterLineEdit->setText(settings.searchFilter);
    dialog->ui->testStackWidget->setCurrentWidget(dialog->ui->testResultPage);
    dialog->ui->testResultLabel->setText(QString());
}

void QnLdapSettingsDialogPrivate::at_timeoutTimer_timeout() {
    int value = dialog->ui->testProgressBar->value();
    if (value < dialog->ui->testProgressBar->maximum()) {
        dialog->ui->testProgressBar->setValue(value + 1);
        return;
    }

    stopTesting(tr("Timed out"));
}

void QnLdapSettingsDialogPrivate::at_serverLineEdit_editingFinished() {
    QUrl url = QUrl::fromUserInput(dialog->ui->serverLineEdit->text());
    if (!url.isValid())
        return;

    if (url.scheme() != lit("ldap") && url.scheme() != lit("ldaps"))
        url.setScheme(lit("ldap"));

    url.setPath(QString());
    dialog->ui->serverLineEdit->setText(url.toString());
}


QnLdapSettingsDialog::QnLdapSettingsDialog(QWidget *parent)
    : base_type(parent)
    , ui(new Ui::LdapSettingsDialog)
    , d(new QnLdapSettingsDialogPrivate(this))
{
    ui->setupUi(this);

    d->testButton = ui->buttonBox->addButton(lit("Test"), QDialogButtonBox::ActionRole);
    connect(d->testButton, &QPushButton::clicked, d, &QnLdapSettingsDialogPrivate::testSettings);

    /* Mark some fields as mandatory, so field label will turn red if the field is empty. */
    auto declareMandatoryField = [this](QLabel* label, QLineEdit* lineEdit) {
        /* On every field text change we should check its contents and repaint label with corresponding color. */
        connect(lineEdit, &QLineEdit::textChanged, this, [this, label](const QString &text) {
            QPalette palette = this->palette();
            if (text.isEmpty())
                setWarningStyle(&palette);
            label->setPalette(palette);
        });
        /* Default field value is empty, so the label should be red by default. */
        setWarningStyle(label);
    };

    declareMandatoryField(ui->serverLabel, ui->serverLineEdit);
    declareMandatoryField(ui->adminDnLabel, ui->adminDnLineEdit);
    declareMandatoryField(ui->passwordLabel, ui->passwordLineEdit);

    connect(ui->serverLineEdit, &QLineEdit::editingFinished, d, &QnLdapSettingsDialogPrivate::at_serverLineEdit_editingFinished);

    d->updateFromSettings();
}

QnLdapSettingsDialog::~QnLdapSettingsDialog() {}

void QnLdapSettingsDialog::at_testLdapSettingsFinished(int status, const QnTestLdapSettingsReply &reply, int handle) {
    if (handle != d->testHandle)
        return;

    d->stopTesting(status != 0 || reply.errorCode != 0 ? tr("Failed") : tr("Success"));
}

void QnLdapSettingsDialog::accept() {
    d->stopTesting();

    QnLdapSettings settings = d->settings();

    if (!settings.isValid()) {
        QMessageBox::critical(this, tr("Error!"), tr("The provided settings are not valid."));
        return;
    }

    QnGlobalSettings::instance()->setLdapSettings(settings);
    base_type::accept();
}

void QnLdapSettingsDialog::reject() {
    if (d->testHandle != -1)
        d->stopTesting();
    else
        base_type::reject();
}
