#include "ldap_settings_dialog.h"
#include "ui_ldap_settings_dialog.h"

#include <QtCore/QTimer>

#include <QtWidgets/QPushButton>

#include <client_core/connection_context_aware.h>

#include <core/resource_management/resource_pool.h>
#include <core/resource/media_server_resource.h>
#include <api/app_server_connection.h>
#include <api/global_settings.h>
#include <common/common_module.h>
#include <utils/common/ldap.h>

#include <ui/style/custom_style.h>
#include <ui/help/help_topic_accessor.h>
#include <ui/help/help_topics.h>

namespace {
    //TODO: #GDM move timeout constant to more common module
    const int testLdapTimeoutMSec = 30 * 1000; //ec2::RESPONSE_WAIT_TIMEOUT_MS;

    /** Special value, used when the user has pressed "Test" button. */
    const int kTestPrepareHandle = 0;

    /** Special value, used when we are no waiting for the test results anymore. */
    const int kTestInvalidHandle = -1;
}

class QnLdapSettingsDialogPrivate: public QObject, public QnConnectionContextAware
{
    Q_DECLARE_PUBLIC(QnLdapSettingsDialog)
    Q_DECLARE_TR_FUNCTIONS(QnLdapSettingsDialogPrivate)

public:
    QnLdapSettingsDialogPrivate(QnLdapSettingsDialog *parent);

    QnLdapSettingsDialog * const q_ptr;
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
    , q_ptr(parent)
    , testHandle(kTestInvalidHandle)
    , timeoutTimer(new QTimer(parent))
{
    connect(qnGlobalSettings, &QnGlobalSettings::ldapSettingsChanged, this, &QnLdapSettingsDialogPrivate::updateFromSettings);
    connect(timeoutTimer, &QTimer::timeout, this, &QnLdapSettingsDialogPrivate::at_timeoutTimer_timeout);
}

void QnLdapSettingsDialogPrivate::testSettings() {
    /* Make sure stopTesting() will work well. */
    testHandle = kTestPrepareHandle;

    QnLdapSettings settings = this->settings();

    if (!settings.isValid()) {
        stopTesting(tr("The provided settings are not valid.") + lit("\n") + tr("Could not perform a test."));
        return;
    }

    // TODO: #dklychkov #3.0 testLdapSettings rest request (on server side) should check all servers.
    QnMediaServerConnectionPtr serverConnection;
    const auto onlineServers = resourcePool()->getAllServers(Qn::Online);
    for (const QnMediaServerResourcePtr server: onlineServers)
    {
        if (!server->getServerFlags().testFlag(Qn::SF_HasPublicIP))
            continue;

        serverConnection = server->apiConnection();
        break;
    }

    if (!serverConnection)
    {
        QnMediaServerResourcePtr server = commonModule()->currentServer();

        if (!server)
        {
            stopTesting(tr("Could not perform a test."));
            return;
        }

        serverConnection = server->apiConnection();
    }

    Q_Q(QnLdapSettingsDialog);

    q->ui->fieldsWidget->setEnabled(false);
    testButton->setEnabled(false);
    q->ui->buttonBox->button(QDialogButtonBox::Ok)->hide();

    q->ui->testProgressBar->setValue(0);
    q->ui->testStackWidget->setCurrentWidget(q->ui->testProgressPage);
    q->ui->testStackWidget->show();

    timeoutTimer->setInterval(testLdapTimeoutMSec / q->ui->testProgressBar->maximum());
    timeoutTimer->start();

    testHandle = serverConnection->testLdapSettingsAsync(settings, q, SLOT(at_testLdapSettingsFinished(int, const QnLdapUsers &,int, const QString &)));
}

void QnLdapSettingsDialogPrivate::showTestResult(const QString &text) {
    Q_Q(QnLdapSettingsDialog);

    q->ui->testResultLabel->setText(text);
    q->ui->testStackWidget->setCurrentWidget(q->ui->testResultPage);
}

void QnLdapSettingsDialogPrivate::stopTesting(const QString &text) {
    /* Check if we have already processed results (e.g. by timeout) */
    if (testHandle == kTestInvalidHandle)
        return;

    Q_Q(QnLdapSettingsDialog);

    timeoutTimer->stop();

    /* We are not waiting the results anymore. */
    testHandle = kTestInvalidHandle;

    q->ui->buttonBox->button(QDialogButtonBox::Ok)->show();
    testButton->setEnabled(true);
    q->ui->fieldsWidget->setEnabled(true);
    showTestResult(text);
}

QnLdapSettings QnLdapSettingsDialogPrivate::settings() const {
    Q_Q(const QnLdapSettingsDialog);

    QnLdapSettings result;

    QUrl url = QUrl::fromUserInput(q->ui->serverLineEdit->text().trimmed());
    if (url.isValid()) {
        if (url.port() == -1)
            url.setPort(QnLdapSettings::defaultPort(url.scheme() == lit("ldaps")));
        result.uri = url;
    }

    result.adminDn = q->ui->adminDnLineEdit->text().trimmed();
    result.adminPassword = q->ui->passwordLineEdit->text().trimmed();
    result.searchBase = q->ui->searchBaseLineEdit->text().trimmed();
    result.searchFilter = q->ui->searchFilterLineEdit->text().trimmed();
    return result;
}

void QnLdapSettingsDialogPrivate::updateFromSettings() {
    Q_Q(QnLdapSettingsDialog);

    stopTesting();

    const QnLdapSettings &settings = qnGlobalSettings->ldapSettings();

    QUrl url = settings.uri;
    if (url.port() == QnLdapSettings::defaultPort(url.scheme() == lit("ldaps")))
        url.setPort(-1);

    q->ui->serverLineEdit->setText(url.toString());
    q->ui->adminDnLineEdit->setText(settings.adminDn.trimmed());
    q->ui->passwordLineEdit->setText(settings.adminPassword.trimmed());
    q->ui->searchBaseLineEdit->setText(settings.searchBase.trimmed());
    q->ui->searchFilterLineEdit->setText(settings.searchFilter.trimmed());
    q->ui->testStackWidget->setCurrentWidget(q->ui->testResultPage);
    q->ui->testResultLabel->setText(QString());
    q->ui->testStackWidget->hide();

    q->resize(q->width(), q->minimumSizeHint().height());
}

void QnLdapSettingsDialogPrivate::at_timeoutTimer_timeout() {
    Q_Q(QnLdapSettingsDialog);

    int value = q->ui->testProgressBar->value();
    if (value < q->ui->testProgressBar->maximum()) {
        q->ui->testProgressBar->setValue(value + 1);
        return;
    }

    stopTesting(tr("Timed Out"));
}

void QnLdapSettingsDialogPrivate::at_serverLineEdit_editingFinished() {
    Q_Q(QnLdapSettingsDialog);

    QUrl url = QUrl::fromUserInput(q->ui->serverLineEdit->text().trimmed());
    if (!url.isValid())
        return;

    if (url.scheme() != lit("ldap") && url.scheme() != lit("ldaps"))
        url.setScheme(lit("ldap"));

    url.setPath(QString());
    q->ui->serverLineEdit->setText(url.toString());
}


QnLdapSettingsDialog::QnLdapSettingsDialog(QWidget *parent)
    : base_type(parent)
    , ui(new Ui::LdapSettingsDialog)
    , d_ptr(new QnLdapSettingsDialogPrivate(this))
{
    Q_D(QnLdapSettingsDialog);

    ui->setupUi(this);

    d->testButton = ui->buttonBox->addButton(tr("Test"), QDialogButtonBox::HelpRole);
    connect(d->testButton, &QPushButton::clicked, d, &QnLdapSettingsDialogPrivate::testSettings);

    auto updateTestButton = [this] {
        Q_D(QnLdapSettingsDialog);
        /* Check if testing in progress. */
        if (d->testHandle >= kTestPrepareHandle)
            return;
        QnLdapSettings settings = d->settings();
        d->testButton->setEnabled(settings.isValid());
    };

    /* Mark some fields as mandatory, so field label will turn red if the field is empty. */
    auto declareMandatoryField = [this, updateTestButton](QLabel* label, QLineEdit* lineEdit) {
        /* On every field text change we should check its contents and repaint label with corresponding color. */
        connect(lineEdit, &QLineEdit::textChanged, this, [this, label, updateTestButton](const QString &text) {
            QPalette palette = this->palette();
            if (text.isEmpty())
                setWarningStyle(&palette);
            label->setPalette(palette);
            updateTestButton();
        });
        /* Default field value is empty, so the label should be red by default. */
        setWarningStyle(label);
    };

    declareMandatoryField(ui->serverLabel, ui->serverLineEdit);
    declareMandatoryField(ui->adminDnLabel, ui->adminDnLineEdit);
    declareMandatoryField(ui->passwordLabel, ui->passwordLineEdit);

    connect(ui->serverLineEdit, &QLineEdit::editingFinished, d, &QnLdapSettingsDialogPrivate::at_serverLineEdit_editingFinished);

    d->updateFromSettings();
    updateTestButton();

    setHelpTopic(this, Qn::UserSettings_LdapIntegration_Help);
}

QnLdapSettingsDialog::~QnLdapSettingsDialog() {}

void QnLdapSettingsDialog::at_testLdapSettingsFinished(int status, const QnLdapUsers &users, int handle, const QString &errorString) {
    Q_D(QnLdapSettingsDialog);

    if (handle != d->testHandle)
        return;

    QString result;
    if (status != 0 || !errorString.isEmpty()) {
        result = tr("Test failed");
    if (!errorString.isEmpty())
        result += lit(" (%1)").arg(errorString);
    } else {
        result = tr("Test completed successfully: %n users found.", 0, users.size());
    }
    d->stopTesting(result);
}

void QnLdapSettingsDialog::accept() {
    Q_D(QnLdapSettingsDialog);

    d->stopTesting();

    QnLdapSettings settings = d->settings();
    qnGlobalSettings->setLdapSettings(settings);
    qnGlobalSettings->synchronizeNow();

    base_type::accept();
}

void QnLdapSettingsDialog::reject() {
    Q_D(QnLdapSettingsDialog);

    if (d->testHandle != kTestInvalidHandle)
        d->stopTesting();
    else
        base_type::reject();
}
