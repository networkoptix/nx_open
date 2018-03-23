#include "archive_length_widget.h"
#include "ui_archive_length_widget.h"

#include <ui/common/aligner.h>
#include <ui/help/help_topic_accessor.h>
#include <ui/help/help_topics.h>

#include <ui/workaround/widgets_signals_workaround.h>
#include <nx/client/desktop/ui/common/checkbox_utils.h>

#include "../redux/camera_settings_dialog_state.h"
#include "../redux/camera_settings_dialog_store.h"

namespace {

static const int kDangerousMinArchiveDays = 5;

} // namespace

namespace nx {
namespace client {
namespace desktop {

ArchiveLengthWidget::ArchiveLengthWidget(QWidget* parent):
    base_type(parent),
    ui(new Ui::ArchiveLengthWidget)
{
    ui->setupUi(this);

    setHelpTopic(this, Qn::CameraSettings_Recording_ArchiveLength_Help);

    m_aligner = new QnAligner(this);
    m_aligner->addWidgets(
        {
            ui->labelMinDays,
            ui->labelMaxDays
        });
}

ArchiveLengthWidget::~ArchiveLengthWidget() = default;

QnAligner* ArchiveLengthWidget::aligner() const
{
    return m_aligner;
}

void ArchiveLengthWidget::setStore(CameraSettingsDialogStore* store)
{
        connect(store, &CameraSettingsDialogStore::stateChanged, this,
        &ArchiveLengthWidget::loadState);

    connect(ui->checkBoxMinArchive, &QCheckBox::stateChanged, store,
        [store](int state)
        {
            store->setMinRecordingDaysAutomatic(state == Qt::Checked);
        });

    connect(ui->spinBoxMinDays, QnSpinboxIntValueChanged, store,
        [store](int value)
        {
            store->setMinRecordingDaysValue(value);
        });

    connect(ui->checkBoxMaxArchive, &QCheckBox::stateChanged, store,
        [store](int state)
        {
            store->setMaxRecordingDaysAutomatic(state == Qt::Checked);
        });

    connect(ui->spinBoxMaxDays, QnSpinboxIntValueChanged, store,
        [store](int value)
        {
            store->setMaxRecordingDaysValue(value);
        });
}

void ArchiveLengthWidget::loadState(const CameraSettingsDialogState& state)
{
    using RecordingDays = CameraSettingsDialogState::RecordingDays;
    const auto load =
        [](QCheckBox* check, QSpinBox* value, const RecordingDays& data)
        {
            ui::CheckboxUtils::setupTristateCheckbox(check, data.same, data.automatic);
            value->setValue(data.absoluteValue);
            value->setEnabled(data.same && !data.automatic);
        };

    load(ui->checkBoxMinArchive, ui->spinBoxMinDays, state.recording.minDays);
    load(ui->checkBoxMaxArchive, ui->spinBoxMaxDays, state.recording.maxDays);
}

/*
void ArchiveLengthWidget::setReadOnly(bool readOnly)
{
    if (m_readOnly == readOnly)
        return;

    using ::setReadOnly;
    setReadOnly(ui->checkBoxMaxArchive, readOnly);
    setReadOnly(ui->checkBoxMinArchive, readOnly);
    setReadOnly(ui->spinBoxMaxDays, readOnly);
    setReadOnly(ui->spinBoxMinDays, readOnly);
    m_readOnly = readOnly;
}

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
