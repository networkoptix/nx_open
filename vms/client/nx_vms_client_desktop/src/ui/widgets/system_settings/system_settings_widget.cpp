// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "system_settings_widget.h"
#include "ui_system_settings_widget.h"

#include <client/client_globals.h>
#include <core/resource/device_dependent_strings.h>
#include <nx/branding.h>
#include <nx/vms/client/core/ui/translation_list_model.h>
#include <nx/vms/client/desktop/help/help_topic.h>
#include <nx/vms/client/desktop/help/help_topic_accessor.h>
#include <nx/vms/client/desktop/style/custom_style.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/common/system_settings.h>
#include <ui/common/read_only.h>

using namespace nx::vms::client::desktop;
using namespace nx::vms::common;

using TranslationListModel = nx::vms::client::core::TranslationListModel;

QnSystemSettingsWidget::QnSystemSettingsWidget(
    nx::vms::api::SaveableSystemSettings* editableSystemSettings,
    QWidget *parent)
    :
    AbstractSystemSettingsWidget(editableSystemSettings, parent),
    ui(new Ui::SystemSettingsWidget)
{
    ui->setupUi(this);

    setHelpTopic(ui->autoDiscoveryCheckBox, HelpTopic::Id::SystemSettings_Server_CameraAutoDiscovery);
    ui->autodiscoveryHint->addHintLine(tr("When enabled, new cameras and servers are continuously "
        "discovered and discovery requests are sent to cameras for status updates."));
    ui->autodiscoveryHint->addHintLine(
        tr("If Failover is enabled, server may still request camera status updates regardless of this setting."));
    setHelpTopic(ui->autodiscoveryHint, HelpTopic::Id::SystemSettings_Server_CameraAutoDiscovery);

    setHelpTopic(ui->statisticsReportCheckBox, HelpTopic::Id::SystemSettings_General_AnonymousUsage);
    ui->statisticsReportHint->addHintLine(tr("Includes information about site, such as cameras models and firmware versions, number of servers, etc."));
    ui->statisticsReportHint->addHintLine(tr("Does not include any personal information and is completely anonymous."));
    setHelpTopic(ui->statisticsReportHint, HelpTopic::Id::SystemSettings_General_AnonymousUsage);

    setWarningStyle(ui->settingsWarningLabel);

    connect(ui->autoSettingsCheckBox,
        &QCheckBox::clicked,
        this,
        [this] { ui->settingsWarningLabel->setVisible(!ui->autoSettingsCheckBox->isChecked()); });

    ui->languageComboBox->setModel(new TranslationListModel(this));

    connect(ui->customNotificationLanguageCheckBox,
        &QCheckBox::clicked,
        this,
        [this](const bool checked) { ui->languageComboBox->setVisible(checked); });

    connect(ui->autoDiscoveryCheckBox,
        &QCheckBox::stateChanged,
        this,
        &QnAbstractPreferencesWidget::hasChangesChanged);
    connect(ui->statisticsReportCheckBox,
        &QCheckBox::stateChanged,
        this,
        &QnAbstractPreferencesWidget::hasChangesChanged);
    connect(ui->autoSettingsCheckBox,
        &QCheckBox::stateChanged,
        this,
        &QnAbstractPreferencesWidget::hasChangesChanged);

    connect(ui->customNotificationLanguageCheckBox,
        &QCheckBox::stateChanged,
        this,
        &QnAbstractPreferencesWidget::hasChangesChanged);
    connect(ui->languageComboBox,
        qOverload<int>(&QComboBox::currentIndexChanged),
        this,
        &QnAbstractPreferencesWidget::hasChangesChanged);

    // Let's assume these options are changed so rare, that we can safely drop unsaved changes.
    connect(systemSettings(), &SystemSettings::autoDiscoveryChanged, this,
        &QnSystemSettingsWidget::loadDataToUi);
    connect(systemSettings(), &SystemSettings::cameraSettingsOptimizationChanged, this,
       &QnSystemSettingsWidget::loadDataToUi);
    connect(systemSettings(), &SystemSettings::statisticsAllowedChanged, this,
       &QnSystemSettingsWidget::loadDataToUi);
    connect(systemSettings(), &SystemSettings::cloudSettingsChanged, this,
        &QnSystemSettingsWidget::loadDataToUi);
    connect(systemSettings(), &SystemSettings::cloudNotificationsLanguageChanged, this,
        &QnSystemSettingsWidget::loadDataToUi);

    retranslateUi();
}

QnSystemSettingsWidget::~QnSystemSettingsWidget() = default;

void QnSystemSettingsWidget::retranslateUi()
{
    ui->autoDiscoveryCheckBox->setText(QnDeviceDependentStrings::getDefaultNameFromSet(
        resourcePool(),
        tr("Enable devices and servers autodiscovery and automated device status check"),
        tr("Enable cameras and servers autodiscovery and automated camera status check")));

    ui->autoSettingsCheckBox->setText(QnDeviceDependentStrings::getDefaultNameFromSet(
        resourcePool(),
        tr("Allow optimize device settings"),
        tr("Allow optimize camera settings")));
}

void QnSystemSettingsWidget::loadDataToUi()
{
    ui->autoDiscoveryCheckBox->setChecked(systemSettings()->isAutoDiscoveryEnabled());
    ui->autoSettingsCheckBox->setChecked(systemSettings()->isCameraSettingsOptimizationEnabled());
    ui->settingsWarningLabel->setVisible(false);
    ui->statisticsReportCheckBox->setChecked(systemSettings()->isStatisticsAllowed());

    const bool connectedToCloud = !systemSettings()->cloudSystemId().isEmpty();
    const bool hasCustomLanguage = !systemSettings()->cloudNotificationsLanguage().isEmpty();

    ui->customNotificationLanguageCheckBox->setVisible(connectedToCloud);
    ui->customNotificationLanguageCheckBox->setChecked(hasCustomLanguage);
    ui->languageComboBox->setVisible(connectedToCloud && hasCustomLanguage);

    if (const auto translationModel =
        qobject_cast<TranslationListModel*>(ui->languageComboBox->model());
        NX_ASSERT(translationModel))
    {
        const int currentLanguageIndex = translationModel->localeIndex(
            systemSettings()->cloudNotificationsLanguage());
        ui->languageComboBox->setCurrentIndex(currentLanguageIndex);
    }
}

void QnSystemSettingsWidget::applyChanges()
{
    if (!hasChanges())
        return;

    editableSystemSettings->autoDiscoveryEnabled = ui->autoDiscoveryCheckBox->isChecked();
    editableSystemSettings->cameraSettingsOptimization = ui->autoSettingsCheckBox->isChecked();
    editableSystemSettings->statisticsAllowed = ui->statisticsReportCheckBox->isChecked();
    ui->settingsWarningLabel->setVisible(false);
    if (!systemSettings()->cloudSystemId().isEmpty())
    {
        const auto& locale = ui->languageComboBox->currentData(
            TranslationListModel::LocaleCodeRole).toString();

        editableSystemSettings->cloudNotificationsLanguage =
            ui->customNotificationLanguageCheckBox->isChecked() ? locale : QString{};
    }
}

bool QnSystemSettingsWidget::hasChanges() const
{
    if (isReadOnly())
        return false;

    if (ui->autoDiscoveryCheckBox->isChecked() != systemSettings()->isAutoDiscoveryEnabled())
        return true;

    if (ui->autoSettingsCheckBox->isChecked()
        != systemSettings()->isCameraSettingsOptimizationEnabled())
    {
        return true;
    }

    if (ui->statisticsReportCheckBox->isChecked() != systemSettings()->isStatisticsAllowed())
        return true;

    if (!systemSettings()->cloudSystemId().isEmpty())
    {
        const auto& currentLocale = systemSettings()->cloudNotificationsLanguage();

        if (ui->customNotificationLanguageCheckBox->isChecked() != !currentLocale.isEmpty())
            return true;

        const auto& selectedLocale = ui->languageComboBox->currentData(
            TranslationListModel::LocaleCodeRole).toString();
        if (ui->customNotificationLanguageCheckBox->isChecked() && selectedLocale != currentLocale)
            return true;
    }

    return false;
}

QWidget* QnSystemSettingsWidget::languageComboBox() const
{
    return ui->languageComboBox;
}

void QnSystemSettingsWidget::setReadOnlyInternal(bool readOnly)
{
    using ::setReadOnly;

    setReadOnly(ui->autoDiscoveryCheckBox, readOnly);
    setReadOnly(ui->autoSettingsCheckBox, readOnly);
    setReadOnly(ui->statisticsReportCheckBox, readOnly);
    setReadOnly(ui->customNotificationLanguageCheckBox, readOnly);
    setReadOnly(ui->languageComboBox, readOnly);
}
