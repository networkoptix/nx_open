// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "archive_length_widget.h"
#include "ui_archive_length_widget.h"

#include <nx/utils/metatypes.h>
#include <nx/utils/scoped_connections.h>
#include <nx/vms/client/desktop/common/utils/aligner.h>
#include <nx/vms/client/desktop/common/utils/check_box_utils.h>
#include <nx/vms/client/desktop/common/utils/spin_box_utils.h>
#include <nx/vms/client/desktop/help/help_topic.h>
#include <nx/vms/client/desktop/help/help_topic_accessor.h>
#include <nx/vms/text/time_strings.h>
#include <ui/common/read_only.h>
#include <ui/workaround/widgets_signals_workaround.h>

#include "../data/recording_period.h"
#include "../flux/camera_settings_dialog_state.h"
#include "../flux/camera_settings_dialog_store.h"

using namespace std::chrono;

namespace {

static constexpr int kInvalidIndex = -1;

enum DurationUnitItemRole
{
    UnitSuffixRole = Qt::UserRole,
    UnitDurationRole,
};

} // namespace

namespace nx::vms::client::desktop {

//-------------------------------------------------------------------------------------------------
// ArchiveLengthWidget::Private declaration

struct ArchiveLengthWidget::Private
{
    ArchiveLengthWidget* const q;

    std::unique_ptr<Ui::ArchiveLengthWidget> ui = std::make_unique<Ui::ArchiveLengthWidget>();
    Aligner* const aligner = new Aligner(q);
    nx::utils::ScopedConnections storeConnections;

    void setupUi();

    void loadState(const CameraSettingsDialogState& state);
    void setStore(CameraSettingsDialogStore* store);

    static seconds getPeriodDuration(
        QSpinBox* valueSpinBox,
        QComboBox* unitComboBox);

    static void setPeriodDataToControls(
        QSpinBox* valueSpinBox,
        QComboBox* unitComboBox,
        QCheckBox* autoCheckBox,
        const RecordingPeriod& periodData,
        bool stateHasChanges);

    static void addDurationUnitComboBoxItem(
        QComboBox* unitComboBox,
        QnTimeStrings::Suffix unitSuffux,
        seconds unitDuration);

    static void setupDurationUnitCombobox(QComboBox* unitComboBox);

    static void updateDurationUnitItemsTextsForDisplayedValue(
        QComboBox* unitComboBox,
        int displayedValue);

    static int findDurationUnitItemIndexForPeriodDuration(
        QComboBox* unitComboBox,
        seconds periodDuration);
};

//-------------------------------------------------------------------------------------------------
// ArchiveLengthWidget::Private definition.

void ArchiveLengthWidget::Private::setupUi()
{
    ui->setupUi(q);
    setHelpTopic(q, HelpTopic::Id::CameraSettings_Recording_ArchiveLength);

    check_box_utils::autoClearTristate(ui->checkBoxMinArchive);
    check_box_utils::autoClearTristate(ui->checkBoxMaxArchive);

    spin_box_utils::autoClearSpecialValue(
        ui->spinBoxMinPeriod,
        duration_cast<days>(nx::vms::api::kDefaultMinArchivePeriod).count());

    spin_box_utils::autoClearSpecialValue(
        ui->spinBoxMaxPeriod,
        duration_cast<days>(nx::vms::api::kDefaultMaxArchivePeriod).count());

    setupDurationUnitCombobox(ui->comboBoxMinPeriodUnit);
    setupDurationUnitCombobox(ui->comboBoxMaxPeriodUnit);

    aligner->addWidgets({
        ui->labelMinDays,
        ui->labelMaxDays});
}

void ArchiveLengthWidget::Private::loadState(const CameraSettingsDialogState& state)
{
    setPeriodDataToControls(
        ui->spinBoxMinPeriod,
        ui->comboBoxMinPeriodUnit,
        ui->checkBoxMinArchive,
        state.recording.minPeriod,
        state.hasChanges);

    setPeriodDataToControls(
        ui->spinBoxMaxPeriod,
        ui->comboBoxMaxPeriodUnit,
        ui->checkBoxMaxArchive,
        state.recording.maxPeriod,
        state.hasChanges);

    setReadOnly(ui->checkBoxMinArchive, state.readOnly);
    setReadOnly(ui->checkBoxMaxArchive, state.readOnly);

    setReadOnly(ui->spinBoxMaxPeriod, state.readOnly);
    setReadOnly(ui->spinBoxMinPeriod, state.readOnly);

    setReadOnly(ui->comboBoxMinPeriodUnit, state.readOnly);
    setReadOnly(ui->comboBoxMaxPeriodUnit, state.readOnly);
}

void ArchiveLengthWidget::Private::setStore(CameraSettingsDialogStore* store)
{
    storeConnections = {};

    storeConnections << connect(store, &CameraSettingsDialogStore::stateChanged,
        [this](const CameraSettingsDialogState& state) { loadState(state); });

    // Min period controls connections.
    storeConnections << connect(ui->checkBoxMinArchive, &QCheckBox::clicked,
        store, &CameraSettingsDialogStore::setMinRecordingPeriodAutomatic);

    storeConnections << connect(ui->spinBoxMinPeriod, QnSpinboxIntValueChanged,
        [this, store](int value)
        {
            store->setMinRecordingPeriodValue(
                getPeriodDuration(ui->spinBoxMinPeriod, ui->comboBoxMinPeriodUnit));
        });

    storeConnections << connect(ui->comboBoxMinPeriodUnit, QnComboboxCurrentIndexChanged,
        [this, store](int index)
        {
            if (index == kInvalidIndex)
                return;

            store->setMinRecordingPeriodValue(
                getPeriodDuration(ui->spinBoxMinPeriod, ui->comboBoxMinPeriodUnit));
        });

    // Max period controls connections.
    storeConnections << connect(ui->checkBoxMaxArchive, &QCheckBox::clicked,
        store, &CameraSettingsDialogStore::setMaxRecordingPeriodAutomatic);

    storeConnections << connect(ui->spinBoxMaxPeriod, QnSpinboxIntValueChanged,
        [this, store](int value)
        {
            store->setMaxRecordingPeriodValue(
                getPeriodDuration(ui->spinBoxMaxPeriod, ui->comboBoxMaxPeriodUnit));
        });

    storeConnections << connect(ui->comboBoxMaxPeriodUnit, QnComboboxCurrentIndexChanged,
        [this, store](int index)
        {
            if (index == kInvalidIndex)
                return;

            store->setMaxRecordingPeriodValue(
                getPeriodDuration(ui->spinBoxMaxPeriod, ui->comboBoxMaxPeriodUnit));
        });
}

seconds ArchiveLengthWidget::Private::getPeriodDuration(
    QSpinBox* valueSpinBox,
    QComboBox* unitComboBox)
{
    auto unitIndex = unitComboBox->currentIndex();
    if (unitIndex == kInvalidIndex)
        unitIndex = unitComboBox->count() - 1; //< Use 'days' by default.

    const auto unitDuration = unitComboBox->itemData(unitIndex, UnitDurationRole).value<seconds>();
    return unitDuration * valueSpinBox->value();
}

void ArchiveLengthWidget::Private::setPeriodDataToControls(
    QSpinBox* valueSpinBox,
    QComboBox* unitComboBox,
    QCheckBox* autoCheckBox,
    const RecordingPeriod& periodData,
    bool stateHasChanges)
{
    QSignalBlocker comboBoxSignalBlocker(unitComboBox);

    valueSpinBox->setEnabled(periodData.isManualMode());
    unitComboBox->setEnabled(periodData.hasManualPeriodValue());

    const bool doNotChangeDurationUnit =
        stateHasChanges
        && unitComboBox->currentIndex() != kInvalidIndex
        && valueSpinBox->hasFocus();

   const auto unitIndex = doNotChangeDurationUnit
        ? unitComboBox->currentIndex()
        : findDurationUnitItemIndexForPeriodDuration(unitComboBox, periodData.displayValue());

    const auto unitDuration = unitComboBox->itemData(unitIndex, UnitDurationRole).value<seconds>();

    const auto spinboxValue = periodData.displayValue().count() / unitDuration.count();
    spin_box_utils::setupSpinBox(
        valueSpinBox,
        periodData.hasManualPeriodValue(),
        spinboxValue);

    if (periodData.hasManualPeriodValue())
    {
        unitComboBox->setCurrentIndex(unitIndex);
        updateDurationUnitItemsTextsForDisplayedValue(unitComboBox, spinboxValue);
    }
    else
    {
        unitComboBox->setCurrentIndex(kInvalidIndex);
    }

    autoCheckBox->setCheckState(periodData.autoCheckState());
}

void ArchiveLengthWidget::Private::addDurationUnitComboBoxItem(
    QComboBox* unitComboBox,
    QnTimeStrings::Suffix unitSuffux,
    seconds unitDuration)
{
    const auto itemText = QnTimeStrings::fullSuffix(unitSuffux);

    unitComboBox->addItem(itemText);
    const auto itemIndex = unitComboBox->count() - 1;

    unitComboBox->setItemData(itemIndex, QVariant(static_cast<int>(unitSuffux)), UnitSuffixRole);
    unitComboBox->setItemData(itemIndex, QVariant::fromValue(unitDuration), UnitDurationRole);
}

void ArchiveLengthWidget::Private::setupDurationUnitCombobox(QComboBox* unitComboBox)
{
    addDurationUnitComboBoxItem(unitComboBox, QnTimeStrings::Suffix::Minutes, 1min);
    addDurationUnitComboBoxItem(unitComboBox, QnTimeStrings::Suffix::Hours, 1h);
    addDurationUnitComboBoxItem(unitComboBox, QnTimeStrings::Suffix::Days, days(1));
    unitComboBox->setCurrentIndex(kInvalidIndex);
}

void ArchiveLengthWidget::Private::updateDurationUnitItemsTextsForDisplayedValue(
    QComboBox* unitComboBox,
    int displayedValue)
{
    for (int i = 0; i < unitComboBox->count(); ++i)
    {
        const auto unitSuffix =
            static_cast<QnTimeStrings::Suffix>(unitComboBox->itemData(i, UnitSuffixRole).toInt());

        unitComboBox->setItemText(i, QnTimeStrings::fullSuffix(unitSuffix, displayedValue));
    }
}

int ArchiveLengthWidget::Private::findDurationUnitItemIndexForPeriodDuration(
    QComboBox* unitComboBox,
    seconds periodDuration)
{
    if (periodDuration == 0s)
        return unitComboBox->count() - 1; //< Use 'days' by default.

    for (int i = unitComboBox->count() - 1; i >= 0; --i)
    {
        const auto unitDuration = unitComboBox->itemData(i, UnitDurationRole).value<seconds>();
        if (periodDuration >= unitDuration && periodDuration % unitDuration == 0s)
            return i;
    }

    NX_ASSERT(false, "Unexpected recording period duration: not multiple of minutes");
    return 0;
}

//-------------------------------------------------------------------------------------------------
// ArchiveLengthWidget definition.

ArchiveLengthWidget::ArchiveLengthWidget(QWidget* parent):
    base_type(parent),
    d(new Private{this})
{
    d->setupUi();
}

ArchiveLengthWidget::~ArchiveLengthWidget()
{
    // Required here for forward-declared scoped pointer destruction.
}

Aligner* ArchiveLengthWidget::aligner() const
{
    return d->aligner;
}

void ArchiveLengthWidget::setStore(CameraSettingsDialogStore* store)
{
    d->setStore(store);
}

} // namespace nx::vms::client::desktop
