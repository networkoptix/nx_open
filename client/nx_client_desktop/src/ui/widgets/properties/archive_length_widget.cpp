#include "archive_length_widget.h"
#include "ui_archive_length_widget.h"

#include <boost/algorithm/algorithm.hpp>

#include <core/resource/camera_resource.h>

#include <ui/common/read_only.h>
#include <ui/help/help_topic_accessor.h>
#include <ui/help/help_topics.h>

#include <ui/workaround/widgets_signals_workaround.h>
#include <nx/client/desktop/ui/common/checkbox_utils.h>
#include <core/resource/device_dependent_strings.h>
#include <nx_ec/data/api_camera_attributes_data.h>

namespace {
static const int kDangerousMinArchiveDays = 5;
static const int kRecordedDaysDontChange = std::numeric_limits<int>::max();
}

using namespace nx::client::desktop::ui;

QnArchiveLengthWidget::QnArchiveLengthWidget(QWidget* parent):
    base_type(parent),
    QnUpdatable(),
    QnWorkbenchContextAware(parent, InitializationMode::lazy),
    ui(new Ui::ArchiveLengthWidget),
    m_readOnly(false)
{
    ui->setupUi(this);

    setHelpTopic(this, Qn::CameraSettings_Recording_ArchiveLength_Help);

    CheckboxUtils::autoClearTristate(ui->checkBoxMinArchive);
    CheckboxUtils::autoClearTristate(ui->checkBoxMaxArchive);

    auto notifyChanged =
        [this]
        {
            if (!isUpdating())
            emit changed();
        };

    connect(ui->checkBoxMinArchive,
            &QCheckBox::stateChanged,
            this,
            &QnArchiveLengthWidget::updateArchiveRangeEnabledState);
    connect(ui->checkBoxMinArchive,
            &QCheckBox::stateChanged,
            this,
            notifyChanged);

    connect(ui->checkBoxMaxArchive,
            &QCheckBox::stateChanged,
            this,
            &QnArchiveLengthWidget::updateArchiveRangeEnabledState);
    connect(ui->checkBoxMaxArchive,
            &QCheckBox::stateChanged,
            this,
            notifyChanged);

    connect(ui->spinBoxMinDays,
            QnSpinboxIntValueChanged,
            this,
            &QnArchiveLengthWidget::validateArchiveLength);
    connect(ui->spinBoxMinDays,
            QnSpinboxIntValueChanged,
            this,
            notifyChanged);

    connect(ui->spinBoxMaxDays,
            QnSpinboxIntValueChanged,
            this,
            &QnArchiveLengthWidget::validateArchiveLength);
    connect(ui->spinBoxMaxDays,
            QnSpinboxIntValueChanged,
            this,
            notifyChanged);

    updateArchiveRangeEnabledState();
}

QnArchiveLengthWidget::~QnArchiveLengthWidget()
{
}

void QnArchiveLengthWidget::updateFromResources(const QnVirtualCameraResourceList& cameras)
{
    QnUpdatableGuard<QnArchiveLengthWidget> guard(this);

    updateMinDays(cameras);
    updateMaxDays(cameras);
}

void QnArchiveLengthWidget::submitToResources(const QnVirtualCameraResourceList& cameras)
{
    if (m_readOnly)
        return;

    int maxDays = maxRecordedDays();
    int minDays = minRecordedDays();

    for (const auto& camera: cameras) {
        if (maxDays != kRecordedDaysDontChange)
            camera->setMaxDays(maxDays);

        if (minDays != kRecordedDaysDontChange)
            camera->setMinDays(minDays);
    }
}

bool QnArchiveLengthWidget::isReadOnly() const
{
    return m_readOnly;
}

void QnArchiveLengthWidget::setReadOnly(bool readOnly)
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

QString QnArchiveLengthWidget::alert() const
{
    return m_alert;
}

void QnArchiveLengthWidget::setAlert(const QString& alert)
{
    if (m_alert == alert)
        return;

    m_alert = alert;
    emit alertChanged(m_alert);
}

int QnArchiveLengthWidget::maxRecordedDays() const
{
    switch (ui->checkBoxMaxArchive->checkState())
    {
        case Qt::Unchecked: return ui->spinBoxMaxDays->value();
        case Qt::Checked: //automatically manage but save for future use
            return ui->spinBoxMaxDays->value() * -1;
        default: return kRecordedDaysDontChange;
    }
}

int QnArchiveLengthWidget::minRecordedDays() const
{
    switch (ui->checkBoxMinArchive->checkState())
    {
        case Qt::Unchecked: return ui->spinBoxMinDays->value();
        case Qt::Checked: //automatically manage but save for future use
            return ui->spinBoxMinDays->value() * -1;
        default: return kRecordedDaysDontChange;
    }
}

void QnArchiveLengthWidget::validateArchiveLength()
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

    if (alertVisible)
    {
        alertText = QnDeviceDependentStrings::getDefaultNameFromSet(
            resourcePool(),
            tr("High minimum value can lead to archive length decrease on other devices."),
            tr("High minimum value can lead to archive length decrease on other cameras."));
    }

    setAlert(alertText);
}

void QnArchiveLengthWidget::updateArchiveRangeEnabledState()
{
    ui->spinBoxMaxDays->setEnabled(ui->checkBoxMaxArchive->checkState() == Qt::Unchecked);
    ui->spinBoxMinDays->setEnabled(ui->checkBoxMinArchive->checkState() == Qt::Unchecked);
    validateArchiveLength();
}

void QnArchiveLengthWidget::updateMinDays(const QnVirtualCameraResourceList& cameras)
{
    if (cameras.isEmpty())
        return;

    /* Any negative min days value means 'auto'. Storing absolute value to keep previous one. */
    auto calcMinDays = [](int d) { return d == 0 ? ec2::kDefaultMinArchiveDays : qAbs(d); };

    const int minDays = (*std::min_element(cameras.cbegin(), cameras.cend(),
        [calcMinDays](const QnVirtualCameraResourcePtr& l,
            const QnVirtualCameraResourcePtr& r)
        {
            return calcMinDays(l->minDays()) < calcMinDays(
                r->minDays());
        }))->minDays();

        const bool isAuto = minDays <= 0;

        bool sameMinDays = boost::algorithm::all_of(cameras,
            [minDays, isAuto](const QnVirtualCameraResourcePtr& camera)
            {
                return isAuto
                    ? camera->minDays() <= 0
                    : camera->minDays() == minDays;
            });

        CheckboxUtils::setupTristateCheckbox(ui->checkBoxMinArchive, sameMinDays, isAuto);
        ui->spinBoxMinDays->setValue(calcMinDays(minDays));
}

void QnArchiveLengthWidget::updateMaxDays(const QnVirtualCameraResourceList& cameras)
{
    if (cameras.isEmpty())
        return;

    /* Any negative max days value means 'auto'. Storing absolute value to keep previous one. */
    auto calcMaxDays = [](int d) { return d == 0 ? ec2::kDefaultMaxArchiveDays : qAbs(d); };

    const int maxDays = (*std::max_element(cameras.cbegin(),
        cameras.cend(),
        [calcMaxDays](const QnVirtualCameraResourcePtr& l,
            const QnVirtualCameraResourcePtr& r)
        {
            return calcMaxDays(l->maxDays()) < calcMaxDays(
                r->maxDays());
        }))->maxDays();

        const bool isAuto = maxDays <= 0;
        bool sameMaxDays = boost::algorithm::all_of(cameras,
            [maxDays, isAuto](const QnVirtualCameraResourcePtr& camera)
            {
                return isAuto
                    ? camera->maxDays() <= 0
                    : camera->maxDays() == maxDays;
            });

        CheckboxUtils::setupTristateCheckbox(ui->checkBoxMaxArchive, sameMaxDays, isAuto);
        ui->spinBoxMaxDays->setValue(calcMaxDays(maxDays));
}
