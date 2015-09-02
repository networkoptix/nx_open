#include "image_control_widget.h"
#include "ui_image_control_widget.h"

#include <core/resource/camera_resource.h>

#include <ui/common/checkbox_utils.h>
#include <ui/help/help_topic_accessor.h>
#include <ui/help/help_topics.h>

#include <ui/workaround/widgets_signals_workaround.h>

#include <utils/aspect_ratio.h>

namespace {
    /** We are allowing to set fixed rotation to any value with this step. */
    const int rotationDegreesStep = 90;

    /** Maximum rotation value. */
    const int rotationDegreesMax = 359;
}

QnImageControlWidget::QnImageControlWidget(QWidget* parent /* = 0*/):
    QWidget(parent),
    ui(new Ui::ImageControlWidget)
{
    ui->setupUi(this);

    setHelpTopic(ui->fisheyeCheckBox,                                   Qn::CameraSettings_Dewarping_Help);
    setHelpTopic(ui->forceArCheckBox, ui->forceArComboBox,              Qn::CameraSettings_AspectRatio_Help);
    setHelpTopic(ui->forceRotationCheckBox, ui->forceRotationComboBox,  Qn::CameraSettings_Rotation_Help);

    connect(ui->forceArCheckBox, &QCheckBox::stateChanged, this, [this](int state){ ui->forceArComboBox->setEnabled(state == Qt::Checked);} );

    ui->forceArComboBox->addItem(tr("4:3"),  4.0f / 3);
    ui->forceArComboBox->addItem(tr("16:9"), 16.0f / 9);
    ui->forceArComboBox->addItem(tr("1:1"),  1.0f);
    ui->forceArComboBox->setCurrentIndex(0);

    connect(ui->forceRotationCheckBox, &QCheckBox::stateChanged, this, [this](int state){ ui->forceRotationComboBox->setEnabled(state == Qt::Checked);} );
    for (int degrees = 0; degrees < rotationDegreesMax; degrees += rotationDegreesStep)
        ui->forceRotationComboBox->addItem(tr("%1 degrees").arg(degrees), degrees);
    ui->forceRotationComboBox->setCurrentIndex(0);

    connect(ui->forceArCheckBox,        &QCheckBox::stateChanged,               this,   &QnImageControlWidget::changed);
    connect(ui->forceArComboBox,        QnComboboxCurrentIndexChanged,          this,   &QnImageControlWidget::changed);
    connect(ui->forceRotationCheckBox,  &QCheckBox::stateChanged,               this,   &QnImageControlWidget::changed);
    connect(ui->forceRotationComboBox,  QnComboboxCurrentIndexChanged,          this,   &QnImageControlWidget::changed);

    QnCheckbox::autoCleanTristate(ui->fisheyeCheckBox);

    connect(ui->fisheyeCheckBox,        &QCheckBox::stateChanged,               this,   &QnImageControlWidget::changed);
    connect(ui->fisheyeCheckBox,        &QCheckBox::stateChanged,               this,   &QnImageControlWidget::fisheyeChanged);
}

QnImageControlWidget::~QnImageControlWidget()
{

}

void QnImageControlWidget::updateFromResources(const QnVirtualCameraResourceList &cameras) {
    bool allCamerasHasVideo = boost::algorithm::all_of(cameras, [](const QnVirtualCameraResourcePtr &camera) {
        return camera->hasVideo(0); 
    });
    setVisible(allCamerasHasVideo);

    updateAspectRatioFromResources(cameras);
    updateRotationFromResources(cameras);
    updateFisheyeFromResources(cameras);
}

void QnImageControlWidget::submitToResources(const QnVirtualCameraResourceList &cameras) {
    bool allCamerasHasVideo = boost::algorithm::all_of(cameras, [](const QnVirtualCameraResourcePtr &camera) {
        return camera->hasVideo(0); 
    });
    if (!allCamerasHasVideo)
        return;

    bool overrideAr = ui->forceArCheckBox->checkState() == Qt::Checked;
    bool clearAr = ui->forceArCheckBox->checkState() == Qt::Unchecked;
    bool overrideRotation = ui->forceRotationCheckBox->checkState() == Qt::Checked;
    bool clearRotation = ui->forceRotationCheckBox->checkState() == Qt::Unchecked;

    for(const QnVirtualCameraResourcePtr &camera: cameras)  {

        if (ui->fisheyeCheckBox->checkState() != Qt::PartiallyChecked) {
            auto params = camera->getDewarpingParams();
            params.enabled = ui->fisheyeCheckBox->isChecked();
            camera->setDewarpingParams(params);
        }

        if (overrideAr)
            camera->setCustomAspectRatio(ui->forceArComboBox->currentData().toReal());
        else if (clearAr)
            camera->clearCustomAspectRatio();

        if(overrideRotation) 
            camera->setProperty(QnMediaResource::rotationKey(), QString::number(ui->forceRotationComboBox->currentData().toInt()));
        else if (clearRotation && camera->hasProperty(QnMediaResource::rotationKey()))
            camera->setProperty(QnMediaResource::rotationKey(), QString());

    }
}

bool QnImageControlWidget::isFisheye() const {
    return ui->fisheyeCheckBox->isChecked();
}

void QnImageControlWidget::updateAspectRatioFromResources(const QnVirtualCameraResourceList &cameras) {
    qreal arOverride = cameras.empty() ? 0 : cameras.front()->customAspectRatio();
    bool sameArOverride = std::all_of(cameras.cbegin(), cameras.cend(), [arOverride](const QnVirtualCameraResourcePtr &camera) {
        return qFuzzyEquals(camera->customAspectRatio(), arOverride);
    });

    ui->forceArCheckBox->setTristate(!sameArOverride);
    if (sameArOverride) {
        ui->forceArCheckBox->setChecked(!qFuzzyIsNull(arOverride));

        /* Float is important here. */
        float ar = QnAspectRatio::closestStandardRatio(arOverride).toFloat();
        int idx = -1;
        for (int i = 0; i < ui->forceArComboBox->count(); ++i) {
            if (qFuzzyEquals(ar, ui->forceArComboBox->itemData(i).toFloat())) {
                idx = i;
                break;
            }
        }
        ui->forceArComboBox->setCurrentIndex(idx < 0 ? 0 : idx);
    } else {
        ui->forceArComboBox->setCurrentIndex(0);
        ui->forceArCheckBox->setCheckState(Qt::PartiallyChecked);
    }
}

void QnImageControlWidget::updateRotationFromResources(const QnVirtualCameraResourceList &cameras) {
    QString rotationOverride = cameras.empty() ? QString() : cameras.front()->getProperty(QnMediaResource::rotationKey());
    bool sameRotation = std::all_of(cameras.cbegin(), cameras.cend(), [rotationOverride](const QnVirtualCameraResourcePtr &camera) {
        return rotationOverride == camera->getProperty(QnMediaResource::rotationKey());
    });

    ui->forceRotationCheckBox->setTristate(!sameRotation);
    if(sameRotation) {
        ui->forceRotationCheckBox->setChecked(!rotationOverride.isEmpty());
        int idx = qBound(0, rotationOverride.toInt(), rotationDegreesMax) / rotationDegreesStep;
        ui->forceRotationComboBox->setCurrentIndex(idx);
    } else {
        ui->forceRotationComboBox->setCurrentIndex(0);
        ui->forceRotationCheckBox->setCheckState(Qt::PartiallyChecked);
    }
}

void QnImageControlWidget::updateFisheyeFromResources(const QnVirtualCameraResourceList &cameras) {
    bool fisheye = cameras.empty() ? false : cameras.front()->getDewarpingParams().enabled;   
    bool sameFisheye = std::all_of(cameras.cbegin(), cameras.cend(), [fisheye](const QnVirtualCameraResourcePtr &camera) {
        return fisheye == camera->getDewarpingParams().enabled;
    });

    ui->fisheyeCheckBox->setTristate(!sameFisheye);
    ui->fisheyeCheckBox->setCheckState(sameFisheye 
        ? fisheye
        ? Qt::Checked
        : Qt::Unchecked
        : Qt::PartiallyChecked);
}
