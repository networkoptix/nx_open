// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "log_settings_dialog.h"
#include "ui_log_settings_dialog.h"

#include <QtGui/QFont>
#include <QtWidgets/QPushButton>

#include <nx/utils/unicode_chars.h>
#include <nx/vms/client/core/skin/color_theme.h>
#include <nx/vms/client/core/skin/skin.h>
#include <nx/vms/client/desktop/common/utils/aligner.h>
#include <nx/vms/client/desktop/common/utils/combo_box_utils.h>
#include <nx/vms/client/desktop/help/help_handler.h>
#include <nx/vms/client/desktop/help/help_topic_accessor.h>
#include <nx/vms/client/desktop/settings/message_bar_settings.h>
#include <nx/vms/client/desktop/style/custom_style.h>
#include <nx/vms/client/desktop/system_administration/models/logs_management_model.h>
#include <nx/vms/text/human_readable.h>
#include <nx/vms/text/time_strings.h>
#include <ui/workaround/widgets_signals_workaround.h>

namespace nx::vms::client::desktop {

namespace {

const unsigned long long kMegabyte = 1024 * 1024;
const unsigned long long kGigabyte = 1024 * 1024 * 1024;

const std::chrono::seconds kMinute = std::chrono::minutes(1);
const std::chrono::seconds kHour = std::chrono::hours(1);
const std::chrono::seconds kDay = std::chrono::days(1);

const std::chrono::seconds kDontSplitByTime{0};

static const QColor kLight16Color = "#698796";
static const nx::vms::client::core::SvgIconColorer::IconSubstitutions kIconSubstitutions = {
    {QIcon::Normal, {{kLight16Color, "light16"}}},
    {QIcon::Active, {{kLight16Color, "light15"}}},
    {QIcon::Selected, {{kLight16Color, "light17"}}},
};

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

unsigned long long valueFromGuiElements(QSpinBox* spin, QComboBox* combo)
{
    return spin->value() * combo->currentData().toULongLong();
}

} // namespace

ConfigurableLogSettings::ConfigurableLogSettings()
{
}

ConfigurableLogSettings::ConfigurableLogSettings(const nx::vms::api::ServerLogSettings& settings):
    loggingLevel(settings.mainLog.primaryLevel),
    maxVolumeSizeB(settings.maxVolumeSizeB),
    maxFileSizeB(settings.maxFileSizeB),
    maxFileTime(settings.maxFileTimePeriodS),
    splitByTimeState(maxFileTime == kDontSplitByTime ? Qt::Unchecked : Qt::Checked)
{
    // Note: now we read log level from the main log settings only, but store it both for the main
    // and the http loggers.
}

nx::vms::api::ServerLogSettings ConfigurableLogSettings::applyTo(
    const nx::vms::api::ServerLogSettings& settings) const
{
    nx::vms::api::ServerLogSettings result = settings;

    if (loggingLevel)
        result.mainLog.primaryLevel = result.httpLog.primaryLevel = *loggingLevel;

    if (maxVolumeSizeB)
        result.maxVolumeSizeB = *maxVolumeSizeB;

    if (maxFileSizeB)
        result.maxFileSizeB = *maxFileSizeB;

    if (maxFileTime)
        result.maxFileTimePeriodS = *maxFileTime;

    return result;
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
        maxFileTime = std::nullopt;

    if (splitByTimeState != other.splitByTimeState)
        splitByTimeState = Qt::PartiallyChecked;
}

ConfigurableLogSettings ConfigurableLogSettings::defaults()
{
    ConfigurableLogSettings result;
    result.loggingLevel = LogsManagementWatcher::defaultLogLevel();
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

    ui->maxVolumeValue->setSpecialValueText(nx::UnicodeChars::kEnDash);
    ui->splitBySizeValue->setSpecialValueText(nx::UnicodeChars::kEnDash);
    ui->splitByTimeValue->setSpecialValueText(nx::UnicodeChars::kEnDash);

    using namespace nx::vms::text;

    ui->maxVolumeUnits->addItem(
        HumanReadable::digitalSizeUnit(HumanReadable::DigitalSizeUnit::Mega), kMegabyte);
    ui->maxVolumeUnits->addItem(
        HumanReadable::digitalSizeUnit(HumanReadable::DigitalSizeUnit::Giga), kGigabyte);
    ui->maxVolumeHint->addHintLine(
        tr("Must be greater than or equal to the value set for Split File by Size"));

    ui->splitBySizeUnits->addItem(
        HumanReadable::digitalSizeUnit(HumanReadable::DigitalSizeUnit::Mega), kMegabyte);
    ui->splitBySizeUnits->addItem(
        HumanReadable::digitalSizeUnit(HumanReadable::DigitalSizeUnit::Giga), kGigabyte);
    ui->splitBySizeHint->addHintLine(
        tr("Must be less than or equal to the value set for Limit Max Volume"));

    using T = long long; // Fix ambigous type cast from long to QVariant.
    ui->splitByTimeUnits->addItem(
        QnTimeStrings::longSuffix(QnTimeStrings::Suffix::Minutes), (T)kMinute.count());
    ui->splitByTimeUnits->addItem(
        QnTimeStrings::longSuffix(QnTimeStrings::Suffix::Hours), (T)kHour.count());
    ui->splitByTimeUnits->addItem(
        QnTimeStrings::longSuffix(QnTimeStrings::Suffix::Days), (T)kDay.count());

    ui->detailsWarning->init(
        {.text = tr("The selected Logging Level may not be collecting enough information.\n"
        "The Logging Level \"%1\" will provide you with more granular information.")
        .arg(LogsManagementModel::logLevelName(LogsManagementWatcher::defaultLogLevel())),
        .level = BarDescription::BarLevel::Info,
        .isEnabledProperty = &messageBarSettings()->detailsLoggingWarning});

    ui->performanceWarning->init(
        {.text = tr("The selected Logging Level may degrade your system's performance.\n"
        "Remember to return the Logging Level to its default setting "
        "after you have collected enough logs."),
        .level = BarDescription::BarLevel::Warning,
        .isEnabledProperty = &messageBarSettings()->performanceLoggingWarning});

    auto resetButton = ui->buttonBox->button(QDialogButtonBox::Reset);
    resetButton->setIcon(qnSkin->icon("text_buttons/reload_20.svg", kIconSubstitutions));
    resetButton->setFlat(true);
    resetButton->setText(tr("Reset to Default"));
    QFont font = resetButton->font();
    font.setBold(false);
    resetButton->setFont(font);

    auto okButton = ui->buttonBox->button(QDialogButtonBox::Ok);
    setAccentStyle(okButton);

    auto cancelButton = ui->buttonBox->button(QDialogButtonBox::Cancel);

    connect(resetButton, &QPushButton::clicked, this, &LogSettingsDialog::resetToDefault);
    connect(okButton, &QPushButton::clicked, this, &QDialog::accept);
    connect(cancelButton, &QPushButton::clicked, this, &QDialog::reject);

    connect(
        ui->loggingLevel, QnComboboxCurrentIndexChanged,
        this, &LogSettingsDialog::updateWarnings);

    auto updateTimeControls =
        [this]
        {
            const auto enabled = ui->splitByTimeChechBox->isChecked();
            ui->splitByTimeValue->setEnabled(enabled);
            ui->splitByTimeUnits->setEnabled(enabled);
        };

    connect(ui->splitByTimeChechBox, &QCheckBox::toggled, this, updateTimeControls);
    updateTimeControls();

    init({});

    setHelpTopic(this, HelpTopic::Id::CollectingLogs);
}

LogSettingsDialog::~LogSettingsDialog()
{
}

void LogSettingsDialog::init(const QList<LogsManagementUnitPtr>& units)
{
    m_ids.clear();
    m_initialSettings = ConfigurableLogSettings();

    bool hasSettings = false;
    for (auto unit: units)
    {
        m_ids << unit->id();

        if (auto settings = unit->settings())
        {
            if (hasSettings)
            {
                m_initialSettings.intersectWith(*settings);
            }
            else
            {
                m_initialSettings = *settings;
                hasSettings = true;
            }
        }
    }

    loadDataToUi(m_initialSettings);
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
    ui->detailsWarning->hide();
    ui->performanceWarning->hide();

    if (auto level = loggingLevel())
    {
        ui->detailsWarning->setVisible(*level < LogsManagementWatcher::defaultLogLevel()
            && messageBarSettings()->detailsLoggingWarning.value());
        ui->performanceWarning->setVisible(*level > LogsManagementWatcher::defaultLogLevel()
            && messageBarSettings()->performanceLoggingWarning.value());
    }
}

void LogSettingsDialog::loadDataToUi(const ConfigurableLogSettings& settings)
{
    setUpdatesEnabled(false);

    setLoggingLevel(settings.loggingLevel);
    setMaxVolumeSize(settings.maxVolumeSizeB);
    setMaxFileSize(settings.maxFileSizeB);
    setMaxFileTime(settings.maxFileTime);

    ui->splitByTimeChechBox->setCheckState(settings.splitByTimeState);

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
    if (auto value = valueFromGuiElements(ui->maxVolumeValue, ui->maxVolumeUnits))
        return value;

    return {}; //< Keep the original value.
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
    if (auto value = valueFromGuiElements(ui->splitBySizeValue, ui->splitBySizeUnits))
        return value;

    return {}; //< Keep the original value.
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
    if (!ui->splitByTimeChechBox->isChecked())
        return kDontSplitByTime;

    if (auto value = valueFromGuiElements(ui->splitByTimeValue, ui->splitByTimeUnits))
        return std::chrono::seconds(value);

    return {}; //< Keep the original value.
}

void LogSettingsDialog::setMaxFileTime(ConfigurableLogSettings::Time time)
{
    ui->splitByTimeValue->setValue(0);
    ui->splitByTimeUnits->setCurrentIndex(0);

    if (time)
    {
        setValueToGuiElements(
            std::chrono::seconds(*time).count(), ui->splitByTimeValue, ui->splitByTimeUnits);
    }
}

} // namespace nx::vms::client::desktop
