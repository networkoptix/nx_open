#include "ldap_settings_widget.h"
#include "ui_ldap_settings_widget.h"

#include <QtWidgets/QMessageBox>

#include <api/app_server_connection.h>
#include <api/global_settings.h>
#include "api/model/test_ldap_settings_reply.h"

#include <nx_ec/ec_api.h>

#include <ui/style/warning_style.h>

#include <ui/actions/actions.h>
#include <ui/actions/action_manager.h>
#include <ui/actions/action_parameters.h>
#include <ui/help/help_topic_accessor.h>
#include <ui/help/help_topics.h>
#include <ui/workaround/widgets_signals_workaround.h>

#include <utils/common/app_info.h>
#include <utils/common/ldap.h>
#include <utils/common/scoped_value_rollback.h>

#include "core/resource_management/resource_pool.h"
#include "core/resource/media_server_resource.h"

namespace {
    enum WidgetPages {
        AdvancedPage,
        TestingPage
    };

    QList<QnEmail::ConnectionType> connectionTypesAllowed() {
        return QList<QnEmail::ConnectionType>() 
            << QnEmail::Unsecure
            << QnEmail::Ssl
            << QnEmail::Tls;
    }

    const int testLdapTimeoutMSec = 20 * 1000;
}

class QnPortNumberValidator: public QIntValidator {
    typedef QIntValidator base_type;
public:
    QnPortNumberValidator(const QString &autoString, QObject* parent = 0):
        base_type(parent), m_autoString(autoString) {}

    virtual QValidator::State validate(QString &input, int &pos) const override {
        if (m_autoString.compare(input, Qt::CaseInsensitive) == 0)
            return QValidator::Acceptable;

        if (m_autoString.startsWith(input, Qt::CaseInsensitive))
            return QValidator::Intermediate;

        QValidator::State result = base_type::validate(input, pos);
        if (result == QValidator::Acceptable && (input.toInt() == 0 || input.toInt() > 65535))
            return QValidator::Intermediate;
        return result;
    }

    virtual void fixup(QString &input) const override {
        if (m_autoString.compare(input, Qt::CaseInsensitive) == 0)
            return;
        if (input.toInt() == 0 || input.toInt() > USHRT_MAX)
            input = m_autoString;
    }
private:
    QString m_autoString;
};

QnLdapSettingsWidget::QnLdapSettingsWidget(QWidget *parent) :
    base_type(parent),
    QnWorkbenchContextAware(parent),
    ui(new Ui::LdapSettingsWidget),
    m_testHandle(-1),
    m_updating(false),
    m_timeoutTimer(new QTimer(this))
{
    ui->setupUi(this);

    setHelpTopic(this, Qn::SystemSettings_Server_Mail_Help);

    connect(ui->portComboBox,               QnComboboxCurrentIndexChanged,      this,   &QnLdapSettingsWidget::at_portComboBox_currentIndexChanged);
    connect(ui->testButton,                 &QPushButton::clicked,              this,   &QnLdapSettingsWidget::at_testButton_clicked);
    connect(ui->cancelTestButton,           &QPushButton::clicked,              this,   &QnLdapSettingsWidget::at_cancelTestButton_clicked);
    connect(ui->okTestButton,               &QPushButton::clicked,              this,   &QnLdapSettingsWidget::finishTesting);
    connect(m_timeoutTimer,                 &QTimer::timeout,                   this,   &QnLdapSettingsWidget::at_timer_timeout);
    connect(QnGlobalSettings::instance(),   &QnGlobalSettings::ldapSettingsChanged, this,  &QnLdapSettingsWidget::updateFromSettings);

    m_timeoutTimer->setSingleShot(false);

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


    /* Mark advanced view mandatory fields. */
    declareMandatoryField(ui->serverLabel, ui->serverLineEdit);
    declareMandatoryField(ui->adminDnLabel, ui->userLineEdit);
    declareMandatoryField(ui->passwordLabel, ui->passwordLineEdit);

    QN_SCOPED_VALUE_ROLLBACK(&m_updating, true);
    const QString autoPort = tr("Auto");
    ui->portComboBox->addItem(autoPort, 0);
    for (QnEmail::ConnectionType type: connectionTypesAllowed()) {
        int port = QnLdapSettings::defaultPort();
        ui->portComboBox->addItem(QString::number(port), port);
    }
    ui->portComboBox->setValidator(new QnPortNumberValidator(autoPort, this));
}

QnLdapSettingsWidget::~QnLdapSettingsWidget()
{
}

void QnLdapSettingsWidget::updateFromSettings() {
    finishTesting();

    QnLdapSettings settings = QnGlobalSettings::instance()->ldapSettings();

    QN_SCOPED_VALUE_ROLLBACK(&m_updating, true);
    loadSettings(settings.host, settings.port);
    ui->userLineEdit->setText(settings.adminDn);
    ui->passwordLineEdit->setText(settings.adminPassword);
    ui->stackedWidget->setCurrentIndex(AdvancedPage);
}

void QnLdapSettingsWidget::submitToSettings() {
    QnGlobalSettings::instance()->setLdapSettings(settings());
}

bool QnLdapSettingsWidget::confirm() {
    finishTesting();
    return base_type::confirm();
}

bool QnLdapSettingsWidget::discard() {
    finishTesting();
    return base_type::discard();
}

QnLdapSettings QnLdapSettingsWidget::settings() const {

    QnLdapSettings result;

    result.host = ui->serverLineEdit->text();
    result.port = ui->portComboBox->currentText().toInt();
    result.adminDn = ui->userLineEdit->text();
    result.adminPassword = ui->passwordLineEdit->text();
    return result;
}

void QnLdapSettingsWidget::stopTesting(const QString &result) {
    if (m_testHandle < 0)
        return;
    m_timeoutTimer->stop();
    m_testHandle = -1;

    ui->testResultLabel->setText(result);
    ui->testProgressBar->setValue(ui->testProgressBar->maximum());
    ui->cancelTestButton->setVisible(false);
    ui->okTestButton->setVisible(true);
}

void QnLdapSettingsWidget::finishTesting() {
    stopTesting();
    ui->controlsWidget->setEnabled(true);
    if (ui->stackedWidget->currentIndex() == TestingPage)
        ui->stackedWidget->setCurrentIndex(AdvancedPage);
}

void QnLdapSettingsWidget::loadSettings(const QString &server, int port) {
    ui->serverLineEdit->setText(server);

    bool portFound = false;
    for (int i = 0; i < ui->portComboBox->count(); i++) {
        if (ui->portComboBox->itemData(i).toInt() == port) {
            ui->portComboBox->setCurrentIndex(i);
            portFound = true;
            break;
        }
    }
    if (!portFound) {
        ui->portComboBox->setEditText(QString::number(port));
    }
}

void QnLdapSettingsWidget::at_portComboBox_currentIndexChanged(int index) {
    if (m_updating)
        return;
}

void QnLdapSettingsWidget::at_testButton_clicked() {
    QnLdapSettings result = settings();
    // result.timeout = testLdapTimeoutMSec / 1000;

    if (!result.isValid()) {
        QMessageBox::warning(this, tr("Invalid data"), tr("Provided parameters are not valid. Could not perform a test."));
        return;
    }

    QnMediaServerConnectionPtr serverConnection;
    for(const QnMediaServerResourcePtr server: qnResPool->getAllServers()) {
        if (server->getStatus() != Qn::Online)
            continue;

        if (!(server->getServerFlags() & Qn::SF_HasPublicIP))
            continue;
        
        serverConnection = server->apiConnection();
        break;
    }

    if (!serverConnection) {
        QMessageBox::warning(this, tr("Network Error"), tr("Could not perform a test. None of your servers is connected to the Internet."));
        return;
    }

    ui->controlsWidget->setEnabled(false);

    ui->testServerLabel->setText(result.host);
    ui->testPortLabel->setText(QString::number(389));
    ui->testUserLabel->setText(result.adminDn);

    ui->cancelTestButton->setVisible(true);
    ui->okTestButton->setVisible(false);

    ui->testProgressBar->setValue(0);

    ui->testResultLabel->setText(tr("In Progress..."));

    m_timeoutTimer->setInterval(testLdapTimeoutMSec / ui->testProgressBar->maximum());
    m_timeoutTimer->start();

    ui->stackedWidget->setCurrentIndex(TestingPage);
    m_testHandle = serverConnection->testLdapSettingsAsync( result, this, SLOT(at_testLdapSettingsFinished(int, const QnTestLdapSettingsReply& , int)));       
}

void QnLdapSettingsWidget::at_testLdapSettingsFinished(int status, const QnTestLdapSettingsReply& reply, int handle)
{
    if (handle != m_testHandle)
        return;

    stopTesting(status != 0 || reply.errorCode != 0
        ? tr("Failed")
        : tr("Success") );
}

void QnLdapSettingsWidget::at_cancelTestButton_clicked() {
    stopTesting(tr("Canceled"));
}

void QnLdapSettingsWidget::at_timer_timeout() {
    int value = ui->testProgressBar->value();
    if (value < ui->testProgressBar->maximum()) {
        ui->testProgressBar->setValue(value + 1);
        return;
    }

    stopTesting(tr("Timed out"));
}

bool QnLdapSettingsWidget::hasChanges() const  {
    QnLdapSettings local = settings();
    QnLdapSettings remote = QnGlobalSettings::instance()->ldapSettings();

    /* Do not notify about changes if no valid settings are provided. */
    if (!local.isValid() && !remote.isValid())
        return false;

    return !local.equals(remote);
}

