#include "image_control_widget.h"
#include "ui_image_control_widget.h"

#include <core/resource/camera_resource.h>

#include <ui/help/help_topic_accessor.h>
#include <ui/help/help_topics.h>

#include <ui/workaround/widgets_signals_workaround.h>

#include <utils/aspect_ratio.h>

QnImageControlWidget::QnImageControlWidget(QWidget* parent /*= 0*/):
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

    ui->forceRotationComboBox->addItem(tr("0 degrees"),      0);
    ui->forceRotationComboBox->addItem(tr("90 degrees"),    90);
    ui->forceRotationComboBox->addItem(tr("180 degrees"),   180);
    ui->forceRotationComboBox->addItem(tr("270 degrees"),   270);
    ui->forceRotationComboBox->setCurrentIndex(0);

    connect(ui->forceArCheckBox,        &QCheckBox::stateChanged,               this,   &QnImageControlWidget::changed);
    connect(ui->forceArComboBox,        QnComboboxCurrentIndexChanged,          this,   &QnImageControlWidget::changed);
    connect(ui->forceRotationCheckBox,  &QCheckBox::stateChanged,               this,   &QnImageControlWidget::changed);
    connect(ui->forceRotationComboBox,  QnComboboxCurrentIndexChanged,          this,   &QnImageControlWidget::changed);

    connect(ui->fisheyeCheckBox,        &QCheckBox::clicked,                    this,   [this]() {
        Qt::CheckState state = ui->fisheyeCheckBox->checkState();
        ui->fisheyeCheckBox->setTristate(false);
        if (state == Qt::PartiallyChecked)
            ui->fisheyeCheckBox->setCheckState(Qt::Checked);
    });

    connect(ui->fisheyeCheckBox,        &QCheckBox::stateChanged,               this,   &QnImageControlWidget::changed);
    connect(ui->fisheyeCheckBox,        &QCheckBox::stateChanged,               this,   &QnImageControlWidget::fisheyeChanged);
}

QnImageControlWidget::~QnImageControlWidget()
{

}

void QnImageControlWidget::updateFromResources(const QnVirtualCameraResourceList &cameras) {
    bool hasVideo = std::any_of(cameras.cbegin(), cameras.cend(), [](const QnVirtualCameraResourcePtr &camera) { return camera->hasVideo(0); });
    setEnabled(hasVideo);

    ui->fisheyeCheckBox->setCheckState(Qt::Unchecked);

    /* Update checkbox state based on camera value. */
    auto setupCheckbox = [](QCheckBox* checkBox, bool firstCamera, bool value){
        Qt::CheckState state = value ? Qt::Checked : Qt::Unchecked;
        if (firstCamera) {
            checkBox->setTristate(false);
            checkBox->setCheckState(state);
        }
        else if (state != checkBox->checkState()) {
            checkBox->setTristate(true);
            checkBox->setCheckState(Qt::PartiallyChecked);
        }
    };

    bool firstCamera = true; 

    bool sameArOverride = true;
    qreal arOverride = 0;
    QString rotFirst;
    bool sameRotation = true;

    for (const QnVirtualCameraResourcePtr &camera: cameras) {
        setupCheckbox(ui->fisheyeCheckBox, firstCamera, camera->getDewarpingParams().enabled);

        qreal changedAr = camera->customAspectRatio();
        if (firstCamera) {
            arOverride = changedAr;
        } else {
            sameArOverride &= qFuzzyEquals(changedAr, arOverride);
        }

        QString rotation = camera->getProperty(QnMediaResource::rotationKey());
        if(firstCamera) {
            rotFirst = rotation;
        } else {
            sameRotation &= rotFirst == rotation;
        }

        firstCamera = false;

    }

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

    ui->forceRotationCheckBox->setTristate(!sameRotation);
    if(sameRotation) {
        ui->forceRotationCheckBox->setChecked(!rotFirst.isEmpty());

        int degree = rotFirst.toInt();
        ui->forceRotationComboBox->setCurrentIndex(degree/90);
        int idx = -1;
        for (int i = 0; i < ui->forceRotationComboBox->count(); ++i) {
            if (ui->forceRotationComboBox->itemData(i).toInt() == degree) {
                idx = i;
                break;
            }
        }
        ui->forceRotationComboBox->setCurrentIndex(idx < 0 ? 0 : idx);
    } else {
        ui->forceRotationComboBox->setCurrentIndex(0);
        ui->forceRotationCheckBox->setCheckState(Qt::PartiallyChecked);
    }

}

void QnImageControlWidget::submitToResources(const QnVirtualCameraResourceList &cameras) {
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
