#include "legacy_image_control_widget.h"
#include "ui_legacy_image_control_widget.h"

#include <core/resource/camera_resource.h>
#include <ui/common/read_only.h>
#include <ui/help/help_topic_accessor.h>
#include <ui/help/help_topics.h>
#include <ui/workaround/widgets_signals_workaround.h>

#include <nx/vms/client/desktop/common/utils/aligner.h>
#include <nx/utils/algorithm/same.h>

namespace {

/** We are allowing to set fixed rotation to any value with this step. */
const int rotationDegreesStep = 90;

/** Maximum rotation value. */
const int rotationDegreesMax = 359;

const int kAutoRotation = -1;

} // namespace

namespace nx::vms::client::desktop {

LegacyImageControlWidget::LegacyImageControlWidget(QWidget *parent)
    : QWidget(parent)
    , QnUpdatable()
    , ui(new Ui::LegacyImageControlWidget)
    , m_readOnly(false)
{
    ui->setupUi(this);

    setFocusPolicy(Qt::TabFocus);
    setFocusProxy(ui->aspectRatioComboBox);

    setHelpTopic(ui->aspectRatioComboBox, ui->aspectRatioLabel,
                 Qn::CameraSettings_AspectRatio_Help);
    setHelpTopic(ui->rotationComboBox, ui->rotationLabel,
                 Qn::CameraSettings_Rotation_Help);

    auto notifyAboutChanges = [this]
    {
        if (!isUpdating())
            emit changed();
    };

    connect(ui->aspectRatioComboBox, QnComboboxCurrentIndexChanged,
            this, notifyAboutChanges);
    connect(ui->rotationComboBox, QnComboboxCurrentIndexChanged,
            this, notifyAboutChanges);

    m_aligner = new Aligner(this);
    m_aligner->addWidgets({ui->rotationLabel, ui->aspectRatioLabel});
}

LegacyImageControlWidget::~LegacyImageControlWidget()
{
}

Aligner* LegacyImageControlWidget::aligner() const
{
    return m_aligner;
}

void LegacyImageControlWidget::updateFromResources(
        const QnVirtualCameraResourceList &cameras)
{
    QnUpdatableGuard<LegacyImageControlWidget> guard(this);
    bool allCamerasHaveVideo = true;
    bool hasWearable = false;
    for (const auto& camera: cameras)
    {
        allCamerasHaveVideo &= camera->hasVideo(0);
        hasWearable |= camera->hasFlags(Qn::wearable_camera);
    }

    setVisible(allCamerasHaveVideo);
    ui->aspectRatioLabel->setVisible(!hasWearable);
    ui->aspectRatioComboBox->setVisible(!hasWearable);
    ui->formLayout->setVerticalSpacing(hasWearable ? 0 : 8);

    updateAspectRatioFromResources(cameras);
    updateRotationFromResources(cameras);
}

void LegacyImageControlWidget::submitToResources(
        const QnVirtualCameraResourceList &cameras)
{
    if (m_readOnly)
        return;

    bool allCamerasHasVideo = boost::algorithm::all_of(cameras,
        [](const QnVirtualCameraResourcePtr &camera)
        {
            return camera->hasVideo(0);
        }
    );
    if (!allCamerasHasVideo)
        return;

    bool overrideAspectRatio = !ui->aspectRatioComboBox->currentData().isNull();
    QnAspectRatio aspectRatio = ui->aspectRatioComboBox->currentData().value<QnAspectRatio>();
    bool overrideRotation = !ui->rotationComboBox->currentData().isNull();
    int rotation = ui->rotationComboBox->currentData().toInt();
    QString rotationString = rotation >= 0 ? QString::number(rotation) : QString();

    for (const QnVirtualCameraResourcePtr &camera: cameras)
    {
        if (overrideAspectRatio)
        {
            if (aspectRatio.isValid())
                camera->setCustomAspectRatio(aspectRatio);
            else
                camera->clearCustomAspectRatio();
        }

        if (overrideRotation)
            camera->setProperty(QnMediaResource::rotationKey(), rotationString);
    }
}

bool LegacyImageControlWidget::isReadOnly() const
{
    return m_readOnly;
}

void LegacyImageControlWidget::setReadOnly(bool readOnly)
{
    if (m_readOnly == readOnly)
        return;

    using ::setReadOnly;
    setReadOnly(ui->aspectRatioComboBox, readOnly);
    setReadOnly(ui->rotationComboBox, readOnly);

    m_readOnly = readOnly;
}


void LegacyImageControlWidget::updateAspectRatioFromResources(
        const QnVirtualCameraResourceList &cameras)
{
    QnAspectRatio aspectRatio;
    const bool sameAspectRatio = utils::algorithm::same(cameras.cbegin(), cameras.cend(),
        [](const QnVirtualCameraResourcePtr &camera)
        {
            return camera->customAspectRatio();
        },
        &aspectRatio);

    ui->aspectRatioComboBox->clear();
    ui->aspectRatioComboBox->addItem(
            tr("Auto"), QVariant::fromValue(QnAspectRatio()));
    for (const QnAspectRatio &aspectRatio: QnAspectRatio::standardRatios())
    {
        if (aspectRatio.width() < aspectRatio.height())
            continue;

        ui->aspectRatioComboBox->addItem(
                aspectRatio.toString(), QVariant::fromValue(aspectRatio));
    }
    ui->aspectRatioComboBox->setCurrentIndex(0);

    if (sameAspectRatio)
    {
        if (!aspectRatio.isValid())
        {
            ui->aspectRatioComboBox->setCurrentIndex(0);
        }
        else
        {
            int index = ui->aspectRatioComboBox->findData(QVariant::fromValue(aspectRatio));
            if (index != -1)
                ui->aspectRatioComboBox->setCurrentIndex(index);
        }
    }
    else
    {
        ui->aspectRatioComboBox->insertItem(0, L'<' + tr("multiple values") + L'>');
        ui->aspectRatioComboBox->setCurrentIndex(0);
    }
}

void LegacyImageControlWidget::updateRotationFromResources(
        const QnVirtualCameraResourceList &cameras)
{
    int rotation = cameras.isEmpty()
            ? 0
            : cameras.first()->getProperty(QnMediaResource::rotationKey()).toInt();

    bool sameRotation = std::all_of(cameras.cbegin(), cameras.cend(),
        [rotation](const QnVirtualCameraResourcePtr &camera)
        {
            return rotation == camera->getProperty(QnMediaResource::rotationKey()).toInt();
        }
    );

    ui->rotationComboBox->clear();

    for (int degrees = 0; degrees < rotationDegreesMax; degrees += rotationDegreesStep)
        ui->rotationComboBox->addItem(tr("%1 degrees").arg(degrees), degrees);
    ui->rotationComboBox->setCurrentIndex(0);

    if (sameRotation)
    {
        int index = ui->rotationComboBox->findData(rotation);
        if (index != -1)
            ui->rotationComboBox->setCurrentIndex(index);
    }
    else
    {
        ui->rotationComboBox->insertItem(0, L'<' + tr("multiple values") + L'>');
        ui->rotationComboBox->setCurrentIndex(0);
    }
}

} // namespace nx::vms::client::desktop
