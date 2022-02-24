// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "image_control_widget.h"
#include "ui_image_control_widget.h"

#include <nx/vms/client/desktop/common/utils/aligner.h>
#include <nx/vms/client/desktop/common/utils/combo_box_utils.h>
#include <ui/common/read_only.h>
#include <ui/help/help_topic_accessor.h>
#include <ui/help/help_topics.h>
#include <ui/workaround/widgets_signals_workaround.h>

#include "../flux/camera_settings_dialog_state.h"
#include "../flux/camera_settings_dialog_store.h"

namespace nx::vms::client::desktop {

ImageControlWidget::ImageControlWidget(QWidget* parent):
    QWidget(parent),
    ui(new Ui::ImageControlWidget)
{
    ui->setupUi(this);

    setFocusPolicy(Qt::TabFocus);
    setFocusProxy(ui->aspectRatioComboBox);

    setHelpTopic(
        ui->aspectRatioComboBox,
        ui->aspectRatioLabel,
        Qn::CameraSettings_AspectRatio_Help);
    setHelpTopic(
        ui->rotationComboBox,
        ui->rotationLabel,
        Qn::CameraSettings_Rotation_Help);

    m_aligner = new Aligner(this);
    m_aligner->addWidgets({ui->rotationLabel, ui->aspectRatioLabel});

    combo_box_utils::insertMultipleValuesItem(ui->aspectRatioComboBox);

    ui->aspectRatioComboBox->addItem(
        tr("Auto"),
        QVariant::fromValue(QnAspectRatio()));

    for (const auto& aspectRatio: QnAspectRatio::standardRatios())
    {
        if (aspectRatio.width() < aspectRatio.height())
            continue;

        ui->aspectRatioComboBox->addItem(
            aspectRatio.toString(),
            QVariant::fromValue(aspectRatio));
    }

    combo_box_utils::insertMultipleValuesItem(ui->rotationComboBox);

    for (const auto& rotation: nx::vms::client::core::Rotation::standardRotations())
    {
        ui->rotationComboBox->addItem(
            tr("%1 degrees").arg(static_cast<int>(rotation)),
            QVariant::fromValue(rotation));
    }
}

ImageControlWidget::~ImageControlWidget() = default;

Aligner* ImageControlWidget::aligner() const
{
    return m_aligner;
}

void ImageControlWidget::setStore(CameraSettingsDialogStore* store)
{
    connect(
        store,
        &CameraSettingsDialogStore::stateChanged,
        this,
        &ImageControlWidget::loadState);

    connect(
        ui->aspectRatioComboBox,
        QnComboboxCurrentIndexChanged,
        store,
        [this, store](int index)
        {
            const auto& data = ui->aspectRatioComboBox->itemData(index);
            if (data.isValid())
                store->setCustomAspectRatio(data.value<QnAspectRatio>());
        });

    connect(
        ui->rotationComboBox,
        QnComboboxCurrentIndexChanged,
        store,
        [this, store](int index)
        {
            const auto& data = ui->rotationComboBox->itemData(index);
            if (data.isValid())
                store->setCustomRotation(data.value<nx::vms::client::core::StandardRotation>());
        });
}

void ImageControlWidget::loadState(const CameraSettingsDialogState& state)
{
    const auto& imageControl = state.imageControl;

    ui->aspectRatioLabel->setVisible(imageControl.aspectRatioAvailable);
    ui->aspectRatioComboBox->setVisible(imageControl.aspectRatioAvailable);
    if (imageControl.aspectRatioAvailable)
    {
        const auto data = imageControl.aspectRatio.hasValue()
            ? QVariant::fromValue(imageControl.aspectRatio())
            : QVariant();
        const auto index = ui->aspectRatioComboBox->findData(data);
        ui->aspectRatioComboBox->setCurrentIndex(index);
        setReadOnly(ui->aspectRatioComboBox, state.readOnly);
    }

    ui->rotationLabel->setVisible(imageControl.rotationAvailable);
    ui->rotationComboBox->setVisible(imageControl.rotationAvailable);
    if (imageControl.rotationAvailable)
    {
        const auto data = imageControl.rotation.hasValue()
            ? QVariant::fromValue(imageControl.rotation())
            : QVariant();
        const auto index = ui->rotationComboBox->findData(data);
        ui->rotationComboBox->setCurrentIndex(index);
        setReadOnly(ui->rotationComboBox, state.readOnly);
    }

    setVisible(imageControl.aspectRatioAvailable || imageControl.rotationAvailable);

    if (!isVisible())
        return;

    ui->imageControlGroupBox->layout()->activate();
    layout()->activate();
}

} // namespace nx::vms::client::desktop
