#include "archive_length_widget.h"
#include "ui_archive_length_widget.h"
#include "../redux/camera_settings_dialog_state.h"
#include "../redux/camera_settings_dialog_store.h"

#include <ui/common/read_only.h>
#include <ui/help/help_topic_accessor.h>
#include <ui/help/help_topics.h>
#include <ui/workaround/widgets_signals_workaround.h>

#include <nx/client/desktop/common/utils/aligner.h>
#include <nx/client/desktop/common/utils/check_box_utils.h>
#include <nx/utils/disconnect_helper.h>

namespace {

static const int kDangerousMinArchiveDays = 5;

} // namespace

namespace nx {
namespace client {
namespace desktop {

ArchiveLengthWidget::ArchiveLengthWidget(QWidget* parent):
    base_type(parent),
    ui(new Ui::ArchiveLengthWidget),
    m_aligner(new Aligner(this))
{
    ui->setupUi(this);
    setHelpTopic(this, Qn::CameraSettings_Recording_ArchiveLength_Help);

    check_box_utils::autoClearTristate(ui->checkBoxMinArchive);
    check_box_utils::autoClearTristate(ui->checkBoxMaxArchive);

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
    m_storeConnections.reset(new QnDisconnectHelper());

    *m_storeConnections << connect(store, &CameraSettingsDialogStore::stateChanged,
        this, &ArchiveLengthWidget::loadState);

    *m_storeConnections << connect(ui->checkBoxMinArchive, &QCheckBox::clicked,
        store, &CameraSettingsDialogStore::setMinRecordingDaysAutomatic);

    *m_storeConnections << connect(ui->spinBoxMinDays, QnSpinboxIntValueChanged,
        store, &CameraSettingsDialogStore::setMinRecordingDaysValue);

    *m_storeConnections << connect(ui->checkBoxMaxArchive, &QCheckBox::clicked,
        store, &CameraSettingsDialogStore::setMaxRecordingDaysAutomatic);

    *m_storeConnections << connect(ui->spinBoxMaxDays, QnSpinboxIntValueChanged,
        store, &CameraSettingsDialogStore::setMaxRecordingDaysValue);
}

void ArchiveLengthWidget::loadState(const CameraSettingsDialogState& state)
{
    using RecordingDays = CameraSettingsDialogState::RecordingDays;
    const auto load =
        [](QCheckBox* check, QSpinBox* value, const RecordingDays& data)
        {
            check_box_utils::setupTristateCheckbox(check, data.same, data.automatic);
            value->setValue(data.absoluteValue);
            value->setEnabled(data.same && !data.automatic);
        };

    load(ui->checkBoxMinArchive, ui->spinBoxMinDays, state.recording.minDays);
    load(ui->checkBoxMaxArchive, ui->spinBoxMaxDays, state.recording.maxDays);

    setReadOnly(ui->checkBoxMaxArchive, state.readOnly);
    setReadOnly(ui->checkBoxMinArchive, state.readOnly);
    setReadOnly(ui->spinBoxMaxDays, state.readOnly);
    setReadOnly(ui->spinBoxMinDays, state.readOnly);
}

/*
void ArchiveLengthWidget::validateArchiveLength()
{
    if (ui->checkBoxMinArchive->checkState() == Qt::Unchecked
        && ui->checkBoxMaxArchive->checkState() == Qt::Unchecked)
    {
        if (ui->spinBoxMaxDays->value() < ui->spinBoxMinDays->value())
            ui->spinBoxMaxDays->setValue(ui->spinBoxMinDays->value());
    }

    QString alertText;
    bool alertVisible = ui->spinBoxMinDays->isEnabled() && ui->spinBoxMinDays->value() >
        kDangerousMinArchiveDays;
    /*
    if (alertVisible)
    {
        alertText = QnDeviceDependentStrings::getDefaultNameFromSet(
            resourcePool(),
            tr("High minimum value can lead to archive length decrease on other devices."),
            tr("High minimum value can lead to archive length decrease on other cameras."));
    }

    setAlert(alertText);
}
*/


} // namespace desktop
} // namespace client
} // namespace nx
