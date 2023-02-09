// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "single_target_device_picker_widget.h"


#include <core/resource/camera_resource.h>
#include <core/resource/layout_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/vms/client/desktop/layout/layout_data_helper.h>
#include <nx/vms/client/desktop/resource_dialogs/camera_selection_dialog.h>
#include <nx/vms/client/desktop/style/resource_icon_cache.h>
#include <nx/vms/client/desktop/style/skin.h>
#include <nx/vms/rules/event_filter_fields/source_camera_field.h>
#include <nx/vms/rules/utils/field.h>

#include "picker_widget_strings.h"

namespace nx::vms::client::desktop::rules {

SingleTargetDevicePicker::SingleTargetDevicePicker(SystemContext* context, CommonParamsWidget* parent):
    ResourcePickerWidgetBase<vms::rules::TargetSingleDeviceField>(context, parent)
{
    auto contentLayout = qobject_cast<QVBoxLayout*>(m_contentWidget->layout());

    m_checkBox = new QCheckBox;
    m_checkBox->setText(ResourcePickerWidgetStrings::useEventSourceString());

    contentLayout->addWidget(m_checkBox);

    connect(
        m_checkBox,
        &QCheckBox::stateChanged,
        this,
        &SingleTargetDevicePicker::onStateChanged);
}

void SingleTargetDevicePicker::onActionBuilderChanged()
{
    ResourcePickerWidgetBase<vms::rules::TargetSingleDeviceField>::onActionBuilderChanged();

    m_scopedConnections.reset();

    m_targetLayoutField = parentParamsWidget()->actionBuilder()->
        fieldByName<vms::rules::TargetLayoutField>(vms::rules::utils::kLayoutIdsFieldName);

    if (NX_ASSERT(m_targetLayoutField))
    {
        m_scopedConnections << connect(
            m_targetLayoutField,
            &vms::rules::TargetLayoutField::valueChanged,
            this,
            &SingleTargetDevicePicker::updateUi);
    }
}

void SingleTargetDevicePicker::onSelectButtonClicked()
{
    auto selectedCameras = QnUuidSet{m_field->id()};

    if (!CameraSelectionDialog::selectCameras<QnFullscreenCameraPolicy>(selectedCameras, this))
        return;

    m_field->setId(*selectedCameras.begin());

    updateUi();
}

void SingleTargetDevicePicker::updateUi()
{
    if (m_field->useSource())
    {
        m_selectButton->setText(CameraPickerStrings::sourceCameraString());
        m_selectButton->setIcon(Skin::maximumSizePixmap(
            qnResIconCache->icon(QnResourceIconCache::Camera),
            QIcon::Selected,
            QIcon::Off,
            /*correctDevicePixelRatio*/ false));
    }
    else
    {
        const auto camera =
            resourcePool()->getResourceById<QnVirtualCameraResource>(m_field->id());

        m_selectButton->setText(QnFullscreenCameraPolicy::getText({camera}));

        QIcon icon;
        if (camera)
        {
            icon = cameraExistOnLayouts(camera)
                ? qnResIconCache->icon(camera)
                : qnResIconCache->icon(
                    QnResourceIconCache::Camera | QnResourceIconCache::Incompatible);
        }
        else
        {
            icon = qnSkin->icon("tree/buggy.png");
        }

        m_selectButton->setIcon(Skin::maximumSizePixmap(
            icon,
            QIcon::Selected,
            QIcon::Off,
            /*correctDevicePixelRatio*/ false));
    }

    const QSignalBlocker blocker{m_checkBox};
    m_checkBox->setChecked(m_field->useSource());
}

void SingleTargetDevicePicker::onStateChanged()
{
    m_field->setUseSource(m_checkBox->isChecked());
    updateUi();
}

bool SingleTargetDevicePicker::cameraExistOnLayouts(const QnVirtualCameraResourcePtr& camera)
{
    const auto layouts =
        resourcePool()->getResourcesByIds<QnLayoutResource>(m_targetLayoutField->value());

    return std::all_of(layouts.cbegin(), layouts.cend(),
        [camera](const auto& layout)
        {
            return resourceBelongsToLayout(camera, layout);
        });
}

} // namespace nx::vms::client::desktop::rules
