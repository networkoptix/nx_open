// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "smtp_settings_widget.h"
#include "ui_smtp_settings_widget.h"

#include <QtCore/QScopedValueRollback>

#include <api/global_settings.h>

#include <common/common_module.h>

#include <ui/help/help_topic_accessor.h>
#include <ui/help/help_topics.h>

#include <ui/common/read_only.h>
#include <ui/widgets/system_settings/smtp/smtp_simple_settings_widget.h>
#include <ui/widgets/system_settings/smtp/smtp_advanced_settings_widget.h>
#include <ui/widgets/system_settings/smtp/smtp_test_connection_widget.h>

#include <utils/email/email.h>


namespace {
enum WidgetPages
{
    SimplePage,
    AdvancedPage,
    TestingPage
};

}



QnSmtpSettingsWidget::QnSmtpSettingsWidget(QWidget *parent)
    : base_type(parent)
    , QnWorkbenchContextAware(parent)
    , ui(new Ui::SmtpSettingsWidget)
    , m_simpleSettingsWidget(new QnSmtpSimpleSettingsWidget(this))
    , m_advancedSettingsWidget(new QnSmtpAdvancedSettingsWidget(this))
    , m_testSettingsWidget(new QnSmtpTestConnectionWidget(this))
    , m_updating(false)

{
    ui->setupUi(this);

    setHelpTopic(this, Qn::SystemSettings_Server_Mail_Help);

    ui->stackedWidget->addWidget(m_simpleSettingsWidget);
    ui->stackedWidget->addWidget(m_advancedSettingsWidget);
    ui->stackedWidget->addWidget(m_testSettingsWidget);

    NX_ASSERT(ui->stackedWidget->indexOf(m_simpleSettingsWidget) == SimplePage);
    NX_ASSERT(ui->stackedWidget->indexOf(m_advancedSettingsWidget) == AdvancedPage);
    NX_ASSERT(ui->stackedWidget->indexOf(m_testSettingsWidget) == TestingPage);

    auto checkedChanged = [this]{
        if (!m_updating)
            emit hasChangesChanged();
    };

    connect(ui->advancedCheckBox,       &QCheckBox::toggled,                            this,   &QnSmtpSettingsWidget::at_advancedCheckBox_toggled);
    connect(ui->testButton,             &QPushButton::clicked,                          this,   &QnSmtpSettingsWidget::at_testButton_clicked);
    connect(m_testSettingsWidget,       &QnSmtpTestConnectionWidget::finished,          this,   &QnSmtpSettingsWidget::finishTesting);
    connect(m_simpleSettingsWidget,     &QnSmtpSimpleSettingsWidget::settingsChanged,   this,   checkedChanged);
    connect(m_advancedSettingsWidget,   &QnSmtpAdvancedSettingsWidget::settingsChanged, this,   checkedChanged);

    connect(qnGlobalSettings,           &QnGlobalSettings::emailSettingsChanged,        this,   &QnSmtpSettingsWidget::loadDataToUi);

    connect(this, &QnSmtpSettingsWidget::hasChangesChanged,
        [this]()
        {
            m_passwordChanged = m_passwordChanged
                || m_currentPassword != settings().password;

            m_currentPassword = settings().password;

            if (hasChanges())
            {
                m_simpleSettingsWidget->setHasRemotePassword(false);
                m_advancedSettingsWidget->setHasRemotePassword(false);
            }
        });
}

QnSmtpSettingsWidget::~QnSmtpSettingsWidget()
{
}

void QnSmtpSettingsWidget::loadDataToUi()
{
    QScopedValueRollback<bool> guard(m_updating, true);

    finishTesting();

    QnEmailSettings settings = qnGlobalSettings->emailSettings();

    const bool isEmpty = settings.isEmpty();
    m_advancedSettingsWidget->setHasRemotePassword(!isEmpty);
    m_simpleSettingsWidget->setHasRemotePassword(!isEmpty);

    if (settings.equals(this->settings(), /*compareView*/ true, /*comparePassword*/ false))
        return;

    m_simpleSettingsWidget->setSettings(QnSimpleSmtpSettings::fromSettings(settings));
    m_advancedSettingsWidget->setSettings(settings);

    ui->advancedCheckBox->setChecked(!settings.simple);
    ui->stackedWidget->setCurrentIndex(ui->advancedCheckBox->isChecked()
        ? AdvancedPage
        : SimplePage);
}

void QnSmtpSettingsWidget::applyChanges() {
    finishTesting();
    if (isReadOnly())
        return;

    m_passwordChanged = false;
    m_currentPassword.clear();

    qnGlobalSettings->setEmailSettings(settings());
    qnGlobalSettings->synchronizeNow();
}

void QnSmtpSettingsWidget::setReadOnlyInternal(bool readOnly) {
    using ::setReadOnly;

    setReadOnly(ui->advancedCheckBox, readOnly);
    m_simpleSettingsWidget->setReadOnly(readOnly);
    m_advancedSettingsWidget->setReadOnly(readOnly);
}


QnEmailSettings QnSmtpSettingsWidget::settings() const {
    QnEmailSettings result = ui->advancedCheckBox->isChecked()
        ? m_advancedSettingsWidget->settings()
        : m_simpleSettingsWidget->settings().toSettings(QnEmailSettings());

    result.simple = !ui->advancedCheckBox->isChecked();
    return result;
}

void QnSmtpSettingsWidget::finishTesting() {
    ui->controlsWidget->setEnabled(true);
    if (ui->stackedWidget->currentIndex() == TestingPage)
        ui->stackedWidget->setCurrentIndex(ui->advancedCheckBox->isChecked()
            ? AdvancedPage
            : SimplePage);
}

void QnSmtpSettingsWidget::at_advancedCheckBox_toggled(bool toggled) {
    if (m_updating)
        return;

    QScopedValueRollback<bool> guard(m_updating, true);

    if (toggled) {
        /* Switching from simple to advanced */
        m_advancedSettingsWidget->setSettings(m_simpleSettingsWidget->settings().toSettings(m_advancedSettingsWidget->settings()));
    }
    else {
        /* Switching from advanced to simple */
        m_simpleSettingsWidget->setSettings(QnSimpleSmtpSettings::fromSettings(m_advancedSettingsWidget->settings()));
    }

    ui->stackedWidget->setCurrentIndex(toggled
        ? AdvancedPage
        : SimplePage
        );
}

void QnSmtpSettingsWidget::at_testButton_clicked()
{
    const auto testResult = hasChanges() || !settings().isValid()
        ? m_testSettingsWidget->testSettings(settings())
        : m_testSettingsWidget->testRemoteSettings();

    if (testResult)
    {
        ui->controlsWidget->setEnabled(false);
        ui->stackedWidget->setCurrentIndex(TestingPage);
    }
}

bool QnSmtpSettingsWidget::hasChanges() const
{
    const auto local = settings();
    const auto remote = qnGlobalSettings->emailSettings();

    /* Do not notify about changes if no valid settings are provided. */
    if (!local.isValid() && !remote.isValid())
        return false;

    return m_passwordChanged
        || !local.equals(remote, /*compareView*/ false, /*comparePassword*/ false);
}
