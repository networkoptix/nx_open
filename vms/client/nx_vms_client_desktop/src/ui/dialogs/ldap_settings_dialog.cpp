// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "ldap_settings_dialog.h"
#include "ui_ldap_settings_dialog.h"

#include <QtCore/QTimer>
#include <QtWidgets/QPushButton>

#include <api/server_rest_connection.h>
#include <common/common_module.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/user_resource.h>
#include <nx/utils/guarded_callback.h>
#include <nx/vms/client/core/common/utils/common_module_aware.h>
#include <nx/vms/client/core/network/remote_connection_aware.h>
#include <nx/vms/client/desktop/style/custom_style.h>
#include <nx/vms/common/system_settings.h>
#include <ui/help/help_topic_accessor.h>
#include <ui/help/help_topics.h>
#include <ui/workbench/workbench_context.h>

using namespace nx;
using namespace nx::vms::common;

namespace {
    /** Special value, used when the user has pressed "Test" button. */
    const int kTestPrepareHandle = 0;

    /** Special value, used when we are no waiting for the test results anymore. */
    const int kTestInvalidHandle = -1;
}

class QnLdapSettingsDialogPrivate:
    public QObject,
    public nx::vms::client::core::CommonModuleAware,
    public nx::vms::client::core::RemoteConnectionAware
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
    nx::vms::api::LdapSettings settings() const;
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
    connect(globalSettings(), &SystemSettings::ldapSettingsChanged,
        this, &QnLdapSettingsDialogPrivate::updateFromSettings);
    connect(timeoutTimer, &QTimer::timeout,
        this, &QnLdapSettingsDialogPrivate::at_timeoutTimer_timeout);
}

void QnLdapSettingsDialogPrivate::testSettings()
{
    // Make sure stopTesting() will work well.
    testHandle = kTestPrepareHandle;

    auto settings = this->settings();

    if (!settings.isValid())
    {
        stopTesting(tr("The provided settings are not valid.")
            + '\n'
            + tr("Could not perform a test."));
        return;
    }

    if (!NX_ASSERT(connection()))
    {
        stopTesting(tr("Could not perform a test."));
        return;
    }

    Q_Q(QnLdapSettingsDialog);

    q->ui->fieldsWidget->setEnabled(false);
    testButton->setEnabled(false);
    q->ui->buttonBox->button(QDialogButtonBox::Ok)->hide();

    q->ui->testProgressBar->setValue(0);
    q->ui->testStackWidget->setCurrentWidget(q->ui->testProgressPage);
    q->ui->testStackWidget->show();

    timeoutTimer->setInterval(
        settings.searchTimeoutS.count() * 1000 / q->ui->testProgressBar->maximum());
    timeoutTimer->start();

    testHandle = connectedServerApi()->testLdapSettingsAsync(
        settings,
        nx::utils::guarded(this,
            [q](bool success, int handle, auto users, auto errorString)
            {
                q->at_testLdapSettingsFinished(success, handle, users, errorString);
            }),
        thread());
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

nx::vms::api::LdapSettings QnLdapSettingsDialogPrivate::settings() const
{
    Q_Q(const QnLdapSettingsDialog);

    nx::vms::api::LdapSettings result;

    QUrl url = QUrl::fromUserInput(q->ui->serverLineEdit->text().trimmed());
    if (url.isValid())
    {
        if (url.port() == -1)
            url.setPort(nx::vms::api::LdapSettings::defaultPort(url.scheme() == "ldaps"));
        result.uri = url;
    }

    result.adminDn = q->ui->adminDnLineEdit->text().trimmed();
    result.adminPassword = q->ui->passwordLineEdit->text().trimmed();
    result.searchBase = q->ui->searchBaseLineEdit->text().trimmed();
    result.searchFilter = q->ui->searchFilterLineEdit->text().trimmed();
    result.searchTimeoutS = std::chrono::seconds(q->ui->searchTimeoutSSpinBox->value());
    // TODO: Support searchPageSize.
    return result;
}

void QnLdapSettingsDialogPrivate::updateFromSettings() {
    Q_Q(QnLdapSettingsDialog);

    stopTesting();

    const auto& settings = globalSettings()->ldapSettings();

    QUrl url = settings.uri;
    if (url.port() == nx::vms::api::LdapSettings::defaultPort(url.scheme() == "ldaps"))
        url.setPort(-1);

    q->ui->serverLineEdit->setText(url.toString());
    q->ui->adminDnLineEdit->setText(settings.adminDn.trimmed());
    q->ui->passwordLineEdit->setText(settings.adminPassword.trimmed());
    q->ui->searchBaseLineEdit->setText(settings.searchBase.trimmed());
    q->ui->searchFilterLineEdit->setText(settings.searchFilter.trimmed());
    q->ui->searchTimeoutSSpinBox->setValue(settings.searchTimeoutS.count());
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

    auto updateTestButton =
        [this]()
        {
            Q_D(QnLdapSettingsDialog);
            /* Check if testing in progress. */
            if (d->testHandle >= kTestPrepareHandle)
                return;

            d->testButton->setEnabled(d->settings().isValid());
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
    declareMandatoryField(ui->passwordLabel, ui->passwordLineEdit->lineEdit());

    connect(ui->serverLineEdit, &QLineEdit::editingFinished, d, &QnLdapSettingsDialogPrivate::at_serverLineEdit_editingFinished);

    d->updateFromSettings();
    updateTestButton();

    setWarningStyle(ui->ldapAdminWarningLabel);
    ui->ldapAdminWarningLabel->setText(
        tr("Changing any LDAP settings other than \"Search Filter\" will result in connectivity "
            "loss for all LDAP fetched users."));
    ui->ldapAdminWarningLabel->setVisible(context()->user() && context()->user()->isLdap());

    setHelpTopic(this, Qn::Ldap_Help);
}

QnLdapSettingsDialog::~QnLdapSettingsDialog() {}

void QnLdapSettingsDialog::at_testLdapSettingsFinished(
    bool success, int handle, const nx::vms::api::LdapUserList& users, const QString& errorString)
{
    Q_D(QnLdapSettingsDialog);

    if (handle != d->testHandle)
        return;

    QString result;
    if (!success || !errorString.isEmpty())
    {
        result = tr("Test failed");
        if (!errorString.isEmpty())
        {
            QString refinedErrorString = errorString.trimmed();
            if (refinedErrorString.endsWith('.'))
                refinedErrorString.chop(1);
            result += lit(" (%1)").arg(refinedErrorString);
        }
    }
    else
    {
        result = tr("Test completed successfully: %n users found.", 0, users.size());
    }
    d->stopTesting(result);
}

void QnLdapSettingsDialog::accept() {
    Q_D(QnLdapSettingsDialog);

    d->stopTesting();

    globalSettings()->setLdapSettings(d->settings());
    globalSettings()->synchronizeNow();

    base_type::accept();
}

void QnLdapSettingsDialog::reject() {
    Q_D(QnLdapSettingsDialog);

    if (d->testHandle != kTestInvalidHandle)
        d->stopTesting();
    else
        base_type::reject();
}
