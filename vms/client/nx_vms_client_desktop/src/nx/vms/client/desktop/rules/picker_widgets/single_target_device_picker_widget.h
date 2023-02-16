// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <core/resource/camera_resource.h>
#include <core/resource/layout_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/vms/client/desktop/layout/layout_data_helper.h>
#include <nx/vms/client/desktop/resource_dialogs/camera_selection_dialog.h>
#include <nx/vms/client/desktop/style/resource_icon_cache.h>
#include <nx/vms/client/desktop/style/skin.h>
#include <nx/vms/rules/action_builder_fields/target_layout_field.h>
#include <nx/vms/rules/action_builder_fields/target_single_device_field.h>
#include <nx/vms/rules/event_filter_fields/source_camera_field.h>
#include <nx/vms/rules/utils/field.h>

#include "picker_widget_strings.h"
#include "resource_picker_widget_base.h"

namespace nx::vms::client::desktop::rules {

template<typename Policy>
class SingleTargetDevicePicker:
    public ResourcePickerWidgetBase<vms::rules::TargetSingleDeviceField>
{
public:
    SingleTargetDevicePicker(QnWorkbenchContext* context, CommonParamsWidget* parent):
        ResourcePickerWidgetBase<vms::rules::TargetSingleDeviceField>(context, parent)
    {
        if (!Policy::canUseSourceCamera())
            return;

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

private:
    QCheckBox* m_checkBox{nullptr};

    void onSelectButtonClicked() override
    {
        auto field = theField();
        auto selectedCameras = QnUuidSet{field->id()};

        if (!CameraSelectionDialog::selectCameras<Policy>(selectedCameras, this))
            return;

        field->setId(*selectedCameras.begin());
    }

    void updateUi() override
    {
        auto field = theField();
        if (Policy::canUseSourceCamera() && field->useSource())
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
                resourcePool()->template getResourceById<QnVirtualCameraResource>(field->id());

            m_selectButton->setText(Policy::getText({camera}));
            m_selectButton->setIcon(Skin::maximumSizePixmap(
                icon(camera),
                QIcon::Selected,
                QIcon::Off,
                /*correctDevicePixelRatio*/ false));
        }

        if (Policy::canUseSourceCamera())
        {
            const QSignalBlocker blocker{m_checkBox};
            m_checkBox->setChecked(field->useSource());
        }
    }

    void onStateChanged()
    {
        theField()->setUseSource(m_checkBox->isChecked());
    }

    QIcon icon(const QnVirtualCameraResourcePtr& camera)
    {
        if (!camera)
            return qnSkin->icon("tree/buggy.png");

        if (descriptor()->linkedFields.contains(vms::rules::utils::kLayoutIdsFieldName))
        {
            const auto layoutIdsField = getActionField<vms::rules::TargetLayoutField>(
                vms::rules::utils::kLayoutIdsFieldName);

            if (!NX_ASSERT(layoutIdsField))
                return qnSkin->icon("tree/buggy.png");

            const auto layouts = resourcePool()->template getResourcesByIds<QnLayoutResource>(
                layoutIdsField->value());
            const auto isCameraExistsOnLayouts = std::all_of(
                layouts.cbegin(),
                layouts.cend(),
                [camera](const auto& layout)
                {
                    return resourceBelongsToLayout(camera, layout);
                });

            return isCameraExistsOnLayouts
                ? qnResIconCache->icon(camera)
                : qnResIconCache->icon(
                    QnResourceIconCache::Camera | QnResourceIconCache::Incompatible);
        }

        return qnResIconCache->icon(camera);
    }
};

} // namespace nx::vms::client::desktop::rules
