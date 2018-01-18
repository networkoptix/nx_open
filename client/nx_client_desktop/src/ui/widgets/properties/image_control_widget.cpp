#include "image_control_widget.h"
#include "ui_image_control_widget.h"

#include <core/resource/camera_resource.h>

#include <ui/common/read_only.h>
#include <ui/help/help_topic_accessor.h>
#include <ui/help/help_topics.h>

#include <ui/workaround/widgets_signals_workaround.h>

namespace {
    /** We are allowing to set fixed rotation to any value with this step. */
    const int rotationDegreesStep = 90;

    /** Maximum rotation value. */
    const int rotationDegreesMax = 359;

    const int kAutoRotation = -1;
}

QnImageControlWidget::QnImageControlWidget(QWidget *parent)
    : QWidget(parent)
    , QnUpdatable()
    , ui(new Ui::ImageControlWidget)
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
}

QnImageControlWidget::~QnImageControlWidget()
{

}

void QnImageControlWidget::updateFromResources(
        const QnVirtualCameraResourceList &cameras)
{
    QnUpdatableGuard<QnImageControlWidget> guard(this);
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

    updateAspectRatioFromResources(cameras);
    updateRotationFromResources(cameras);
}

void QnImageControlWidget::submitToResources(
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
                camera->setCustomAspectRatio(aspectRatio.toFloat());
            else
                camera->clearCustomAspectRatio();
        }

        if (overrideRotation)
            camera->setProperty(QnMediaResource::rotationKey(), rotationString);
    }
}

bool QnImageControlWidget::isReadOnly() const
{
    return m_readOnly;
}

void QnImageControlWidget::setReadOnly(bool readOnly)
{
    if (m_readOnly == readOnly)
        return;

    using ::setReadOnly;
    setReadOnly(ui->aspectRatioComboBox, readOnly);
    setReadOnly(ui->rotationComboBox, readOnly);

    m_readOnly = readOnly;
}


void QnImageControlWidget::updateAspectRatioFromResources(
        const QnVirtualCameraResourceList &cameras)
{
    qreal aspectRatio = cameras.empty() ? 0 : cameras.first()->customAspectRatio();
    bool sameAspectRatio = std::all_of(cameras.cbegin(), cameras.cend(),
        [aspectRatio](const QnVirtualCameraResourcePtr &camera)
        {
            return qFuzzyEquals(camera->customAspectRatio(), aspectRatio);
        }
    );

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
        if (qFuzzyIsNull(aspectRatio))
        {
            ui->aspectRatioComboBox->setCurrentIndex(0);
        }
        else
        {
            QnAspectRatio closest = QnAspectRatio::closestStandardRatio(aspectRatio);
            int index = ui->aspectRatioComboBox->findData(QVariant::fromValue(closest));
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

void QnImageControlWidget::updateRotationFromResources(
        const QnVirtualCameraResourceList &cameras)
{
    QString rotationString = cameras.isEmpty()
            ? QString()
            : cameras.first()->getProperty(QnMediaResource::rotationKey());

    bool sameRotation = std::all_of(cameras.cbegin(), cameras.cend(),
        [rotationString](const QnVirtualCameraResourcePtr &camera)
        {
            return rotationString == camera->getProperty(QnMediaResource::rotationKey());
        }
    );

    ui->rotationComboBox->clear();

    ui->rotationComboBox->addItem(tr("Auto"), kAutoRotation);
    for (int degrees = 0; degrees < rotationDegreesMax; degrees += rotationDegreesStep)
        ui->rotationComboBox->addItem(tr("%1 degrees").arg(degrees), degrees);

    if (sameRotation)
    {
        if (rotationString.isEmpty())
        {
            ui->rotationComboBox->setCurrentIndex(0);
        }
        else
        {
            int index = ui->rotationComboBox->findData(rotationString.toInt());
            if (index != -1)
                ui->rotationComboBox->setCurrentIndex(index);
        }
    }
    else
    {
        ui->rotationComboBox->insertItem(0, L'<' + tr("multiple values") + L'>');
        ui->rotationComboBox->setCurrentIndex(0);
    }
}
