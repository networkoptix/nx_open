#include "archive_length_widget.h"
#include "ui_archive_length_widget.h"
#include "../data/recording_days.h"
#include "../redux/camera_settings_dialog_state.h"
#include "../redux/camera_settings_dialog_store.h"

#include <ui/common/read_only.h>
#include <ui/help/help_topic_accessor.h>
#include <ui/help/help_topics.h>
#include <ui/workaround/widgets_signals_workaround.h>

#include <nx/vms/client/desktop/common/utils/aligner.h>
#include <nx/vms/client/desktop/common/utils/check_box_utils.h>
#include <nx/vms/client/desktop/common/utils/spin_box_utils.h>
#include <nx/utils/scoped_connections.h>

namespace nx::vms::client::desktop {

ArchiveLengthWidget::ArchiveLengthWidget(QWidget* parent):
    base_type(parent),
    ui(new Ui::ArchiveLengthWidget),
    m_aligner(new Aligner(this))
{
    ui->setupUi(this);
    setHelpTopic(this, Qn::CameraSettings_Recording_ArchiveLength_Help);

    check_box_utils::autoClearTristate(ui->checkBoxMinArchive);
    check_box_utils::autoClearTristate(ui->checkBoxMaxArchive);

    spin_box_utils::autoClearSpecialValue(ui->spinBoxMinDays, nx::vms::api::kDefaultMinArchiveDays);
    spin_box_utils::autoClearSpecialValue(ui->spinBoxMaxDays, nx::vms::api::kDefaultMaxArchiveDays);

    m_aligner->addWidgets({
        ui->labelMinDays,
        ui->labelMaxDays});
}

ArchiveLengthWidget::~ArchiveLengthWidget()
{
    // Required here for forward-declared scoped pointer destruction.
}

Aligner* ArchiveLengthWidget::aligner() const
{
    return m_aligner;
}

void ArchiveLengthWidget::setStore(CameraSettingsDialogStore* store)
{
    m_storeConnections = {};

    m_storeConnections << connect(store, &CameraSettingsDialogStore::stateChanged,
        this, &ArchiveLengthWidget::loadState);

    m_storeConnections << connect(ui->checkBoxMinArchive, &QCheckBox::clicked,
        store, &CameraSettingsDialogStore::setMinRecordingDaysAutomatic);

    m_storeConnections << connect(ui->spinBoxMinDays, QnSpinboxIntValueChanged,
        store, &CameraSettingsDialogStore::setMinRecordingDaysValue);

    m_storeConnections << connect(ui->checkBoxMaxArchive, &QCheckBox::clicked,
        store, &CameraSettingsDialogStore::setMaxRecordingDaysAutomatic);

    m_storeConnections << connect(ui->spinBoxMaxDays, QnSpinboxIntValueChanged,
        store, &CameraSettingsDialogStore::setMaxRecordingDaysValue);
}

void ArchiveLengthWidget::loadState(const CameraSettingsDialogState& state)
{
    const auto load =
        [](QCheckBox* checkBox, QSpinBox* valueSpinBox, const RecordingDays& data)
        {
            valueSpinBox->setEnabled(data.isManualMode());
            spin_box_utils::setupSpinBox(valueSpinBox, data.hasManualDaysValue(), data.days());
            checkBox->setCheckState(data.autoCheckState());
        };

    load(ui->checkBoxMinArchive, ui->spinBoxMinDays, state.recording.minDays);
    load(ui->checkBoxMaxArchive, ui->spinBoxMaxDays, state.recording.maxDays);

    setReadOnly(ui->checkBoxMaxArchive, state.readOnly);
    setReadOnly(ui->checkBoxMinArchive, state.readOnly);
    setReadOnly(ui->spinBoxMaxDays, state.readOnly);
    setReadOnly(ui->spinBoxMinDays, state.readOnly);
}

} // namespace nx::vms::client::desktop
