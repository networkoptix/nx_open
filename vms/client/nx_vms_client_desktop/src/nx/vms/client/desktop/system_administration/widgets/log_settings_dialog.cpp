// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "log_settings_dialog.h"
#include "ui_log_settings_dialog.h"

#include <QtWidgets/QPushButton>

#include <nx/vms/client/desktop/common/utils/aligner.h>
#include <nx/vms/client/desktop/style/custom_style.h>
#include <nx/vms/client/desktop/style/skin.h>
#include <nx/vms/client/desktop/system_administration/models/logs_management_model.h>
#include <nx/vms/client/desktop/common/utils/combo_box_utils.h>
#include <nx/vms/text/human_readable.h>
#include <nx/vms/text/time_strings.h>
#include <ui/workaround/widgets_signals_workaround.h>

namespace nx::vms::client::desktop {

namespace {

const auto kDash = QString::fromWCharArray(L"\x2013");

const unsigned long long kMegabyte = 1024 * 1024;
const unsigned long long kGigabyte = 1024 * 1024 * 1024;

const std::chrono::seconds kMinute = std::chrono::minutes(1);
const std::chrono::seconds kHour = std::chrono::hours(1);
const std::chrono::seconds kDay = std::chrono::days(1);

bool setValueToGuiElements(unsigned long long value, QSpinBox* spin, QComboBox* combo)
{
    for (int index = combo->count() - 1; index >= 0; --index)
    {
        const unsigned long long delim = combo->itemData(index).toULongLong();
        if (value % delim == 0)
        {
            combo->setCurrentIndex(index);
            spin->setValue(value / delim);
            return true;
        }
    }

    return false;
}

} // namespace

ConfigurableLogSettings::ConfigurableLogSettings()
{
}

ConfigurableLogSettings::ConfigurableLogSettings(const nx::vms::api::ServerLogSettings& settings):
    loggingLevel(settings.mainLog.primaryLevel),
    maxVolumeSizeB(settings.maxVolumeSizeB),
    maxFileSizeB(settings.maxFileSizeB),
    maxFileTime(settings.maxFileTimePeriodS)
{
    // Note: now we read log level from the main log settings only, but store it both for the main
    // and the http loggers.
}

void ConfigurableLogSettings::applyTo(nx::vms::api::ServerLogSettings* settings) const
{
    if (loggingLevel)
        settings->mainLog.primaryLevel = settings->httpLog.primaryLevel = *loggingLevel;

    if (maxVolumeSizeB)
        settings->maxVolumeSizeB = *maxVolumeSizeB;

    if (maxFileSizeB)
        settings->maxFileSizeB = *maxFileSizeB;

    if (maxFileTime)
        settings->maxFileTimePeriodS = *maxFileTime;
}

void ConfigurableLogSettings::intersectWith(const ConfigurableLogSettings& other)
{
    if (loggingLevel != other.loggingLevel)
        loggingLevel = {};

    if (maxVolumeSizeB != other.maxVolumeSizeB)
        maxVolumeSizeB = {};

    if (maxFileSizeB != other.maxFileSizeB)
        maxFileSizeB = {};

    if (maxFileTime != other.maxFileTime)
        maxFileTime = {};
}

ConfigurableLogSettings ConfigurableLogSettings::defaults()
{
    ConfigurableLogSettings result;
    result.loggingLevel = nx::utils::log::Level::info;
    result.maxVolumeSizeB = 250 * kMegabyte;
    result.maxFileSizeB = 10 * kMegabyte;
    result.maxFileTime = std::chrono::seconds::zero(); //< Disabled by default.
    return result;
}

LogSettingsDialog::LogSettingsDialog(QWidget* parent):
    base_type(parent),
    ui(new Ui::LogSettingsDialog)
{
    ui->setupUi(this);

    auto aligner = new Aligner(this);
    aligner->addWidgets({
        ui->loggingLevelLabel, ui->maxVolumeLabel, ui->splitBySizeLabel, ui->splitByTimeLabel});

    ui->maxVolumeValue->setSpecialValueText(kDash);
    ui->splitBySizeValue->setSpecialValueText(kDash);
    ui->splitByTimeValue->setSpecialValueText(kDash);

    using namespace nx::vms::text;

    ui->maxVolumeUnits->addItem(
        HumanReadable::digitalSizeUnit(HumanReadable::DigitalSizeUnit::Mega), kMegabyte);
    ui->maxVolumeUnits->addItem(
        HumanReadable::digitalSizeUnit(HumanReadable::DigitalSizeUnit::Giga), kGigabyte);

    ui->splitBySizeUnits->addItem(
        HumanReadable::digitalSizeUnit(HumanReadable::DigitalSizeUnit::Mega), kMegabyte);
    ui->splitBySizeUnits->addItem(
        HumanReadable::digitalSizeUnit(HumanReadable::DigitalSizeUnit::Giga), kGigabyte);

    using T = long long; // Fix ambigous type cast from long to QVariant.
    ui->splitByTimeUnits->addItem(
        QnTimeStrings::longSuffix(QnTimeStrings::Suffix::Minutes), (T)kMinute.count());
    ui->splitByTimeUnits->addItem(
        QnTimeStrings::longSuffix(QnTimeStrings::Suffix::Hours), (T)kHour.count());
    ui->splitByTimeUnits->addItem(
        QnTimeStrings::longSuffix(QnTimeStrings::Suffix::Days), (T)kDay.count());

    ui->detailsWarning->setText(tr(
        "Notice: Not enough information could be collected on the current Logging level.\n"
        "The Logging level \"info\" will provide more granular information."));
    ui->performanceWarning->setText(tr(
        "Notice: The selected Logging level may degrade the performance of the system.\n"
        "Do not forget to return the Logging level to its default setting after you have collected enough logs."));

    auto resetButton = ui->buttonBox->button(QDialogButtonBox::Reset);
    resetButton->setIcon(qnSkin->icon("text_buttons/refresh.png"));
    resetButton->setFlat(true);
    resetButton->setText(tr("Reset to Default"));

    auto okButton = ui->buttonBox->button(QDialogButtonBox::Ok);
    setAccentStyle(okButton);

    auto cancelButton = ui->buttonBox->button(QDialogButtonBox::Cancel);

    connect(resetButton, &QPushButton::clicked, this, &LogSettingsDialog::resetToDefault);
    connect(okButton, &QPushButton::clicked, this, &QDialog::accept);
    connect(cancelButton, &QPushButton::clicked, this, &QDialog::reject);

    connect(
        ui->loggingLevel, QnComboboxCurrentIndexChanged,
        this, &LogSettingsDialog::updateWarnings);

    init({}, {});
}

LogSettingsDialog::~LogSettingsDialog()
{
}

void LogSettingsDialog::init(const QList<QnUuid>& ids, const ConfigurableLogSettings& settings)
{
    m_ids = ids;
    m_initialSettings = settings;

    loadDataToUi(settings);
}

bool LogSettingsDialog::hasChanges() const
{
    return m_initialSettings.loggingLevel != loggingLevel()
        || m_initialSettings.maxVolumeSizeB != maxVolumeSize()
        || m_initialSettings.maxFileSizeB != maxFileSize()
        || m_initialSettings.maxFileTime != maxFileTime();
}

ConfigurableLogSettings LogSettingsDialog::changes() const
{
    ConfigurableLogSettings result;
    result.loggingLevel = loggingLevel();
    result.maxVolumeSizeB = maxVolumeSize();
    result.maxFileSizeB = maxFileSize();
    result.maxFileTime = maxFileTime();
    return result;
}

void LogSettingsDialog::resetToDefault()
{
    loadDataToUi(ConfigurableLogSettings::defaults());
}

void LogSettingsDialog::updateWarnings()
{
    if (auto level = loggingLevel())
    {
        switch (*level)
        {
            case nx::utils::log::Level::none:
            case nx::utils::log::Level::error:
            case nx::utils::log::Level::warning:
                ui->detailsWarning->show();
                ui->performanceWarning->hide();
                return;

            case nx::utils::log::Level::debug:
            case nx::utils::log::Level::verbose:
                ui->detailsWarning->hide();
                ui->performanceWarning->show();
                return;
        }
    }

    ui->detailsWarning->hide();
    ui->performanceWarning->hide();
}

void LogSettingsDialog::loadDataToUi(const ConfigurableLogSettings& settings)
{
    setUpdatesEnabled(false);

    setLoggingLevel(settings.loggingLevel);
    setMaxVolumeSize(settings.maxVolumeSizeB);
    setMaxFileSize(settings.maxFileSizeB);
    setMaxFileTime(settings.maxFileTime);

    setUpdatesEnabled(true);
}

ConfigurableLogSettings::Level LogSettingsDialog::loggingLevel() const
{
    if (auto data = ui->loggingLevel->currentData(); data.isValid())
        return (nx::utils::log::Level)data.toInt();

    return {};
}

void LogSettingsDialog::setLoggingLevel(ConfigurableLogSettings::Level level)
{
    ui->loggingLevel->clear();

    int index = 0;

    combo_box_utils::insertMultipleValuesItem(ui->loggingLevel);

    for (const auto l: LogsManagementModel::logLevels())
    {
        if (level == l)
            index = ui->loggingLevel->count();

        ui->loggingLevel->addItem(LogsManagementModel::logLevelName(l), (int)l);
    }

    ui->loggingLevel->setCurrentIndex(index);
}

ConfigurableLogSettings::Size LogSettingsDialog::maxVolumeSize() const
{
    return ui->maxVolumeValue->value() * ui->maxVolumeUnits->currentData().toULongLong();
}

void LogSettingsDialog::setMaxVolumeSize(ConfigurableLogSettings::Size size)
{
    ui->maxVolumeValue->setValue(0);
    ui->maxVolumeUnits->setCurrentIndex(0);

    if (size)
        setValueToGuiElements(*size, ui->maxVolumeValue, ui->maxVolumeUnits);
}

ConfigurableLogSettings::Size LogSettingsDialog::maxFileSize() const
{
    return ui->splitBySizeValue->value() * ui->splitBySizeUnits->currentData().toULongLong();
}

void LogSettingsDialog::setMaxFileSize(ConfigurableLogSettings::Size size)
{
    ui->splitBySizeValue->setValue(0);
    ui->splitBySizeUnits->setCurrentIndex(0);

    if (size)
        setValueToGuiElements(*size, ui->splitBySizeValue, ui->splitBySizeUnits);
}

ConfigurableLogSettings::Time LogSettingsDialog::maxFileTime() const
{
    if (ui->splitByTimeChechBox->isChecked())
    {
        return std::chrono::seconds(
            ui->splitByTimeValue->value() * ui->splitByTimeUnits->currentData().toULongLong());
    }

    return {};
}

void LogSettingsDialog::setMaxFileTime(ConfigurableLogSettings::Time time)
{
    ui->splitByTimeValue->setValue(0);
    ui->splitByTimeUnits->setCurrentIndex(0);

    if (time)
        setValueToGuiElements(time->count(), ui->splitByTimeValue, ui->splitByTimeUnits);

    ui->splitByTimeChechBox->setChecked(time.has_value());
}

} // namespace nx::vms::client::desktop
