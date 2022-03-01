// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtWidgets/QHBoxLayout>

#include <client_core/client_core_module.h>
#include <common/common_module.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource/camera_resource.h>
#include <core/resource/device_dependent_strings.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/user_resource.h>
#include <nx/vms/client/desktop/style/custom_style.h>
#include <nx/vms/client/desktop/style/helper.h>
#include <nx/vms/rules/event_fields/source_camera_field.h>
#include <nx/vms/rules/event_fields/source_server_field.h>
#include <nx/vms/rules/event_fields/source_user_field.h>
#include <ui/widgets/common/elided_label.h>
#include <ui/widgets/select_resources_button.h>

#include "picker_widget.h"

namespace nx::vms::client::desktop::rules {

/**
 * Used for types that could be represended as a list of ids. Implementation is dependent on the
 * field parameter.
 * Has implementation for:
 * - nx.events.fields.sourceCamera
 * - nx.events.fields.sourceServer
 * - nx.events.fields.sourceUser
 */
template<typename F>
class SourcePickerWidget: public FieldPickerWidget<F>
{
public:
    explicit SourcePickerWidget(QWidget* parent = nullptr):
        FieldPickerWidget<F>(parent)
    {
        auto mainLayout = new QHBoxLayout;
        mainLayout->setSpacing(style::Metrics::kDefaultLayoutSpacing.width());

        label = new QnElidedLabel;
        label->setAlignment(Qt::AlignRight);
        mainLayout->addWidget(label);

        {
            auto selectButtonLayout = new QHBoxLayout;
            selectButtonLayout->setSpacing(style::Metrics::kDefaultLayoutSpacing.width());

            selectResourceButton = createSelectButton<F>();
            selectButtonLayout->addWidget(selectResourceButton);

            alertLabel = new QnElidedLabel;
            setWarningStyle(alertLabel);
            selectButtonLayout->addWidget(alertLabel);

            selectButtonLayout->setStretch(0, 5);
            selectButtonLayout->setStretch(1, 3);

            mainLayout->addLayout(selectButtonLayout);
        }

        mainLayout->setStretch(0, 1);
        mainLayout->setStretch(1, 5);

        setLayout(mainLayout);
    }

    virtual void setReadOnly(bool value) override
    {
        selectResourceButton->setEnabled(!value);
    }

private:
    using FieldPickerWidget<F>::connect;
    using FieldPickerWidget<F>::tr;
    using FieldPickerWidget<F>::setLayout;
    using FieldPickerWidget<F>::edited;
    using FieldPickerWidget<F>::fieldDescriptor;
    using FieldPickerWidget<F>::field;

    QnElidedLabel* label{};
    QnSelectResourcesButton* selectResourceButton{};
    QnElidedLabel* alertLabel{};

    virtual void onDescriptorSet() override
    {
        label->setText(fieldDescriptor->displayName);
    }

    virtual void onFieldSet() override
    {
        setIds<F>(field->ids());

        connect(selectResourceButton, &QPushButton::clicked, this,
            [this]
            {
                // TODO: Open appropriate dialog.
            });
    }

    template<typename T>
    QnSelectResourcesButton* createSelectButton();

    template<>
    QnSelectResourcesButton* createSelectButton<vms::rules::SourceCameraField>()
    {
        return new QnSelectDevicesButton;
    }

    template<>
    QnSelectResourcesButton* createSelectButton<vms::rules::SourceServerField>()
    {
        return new QnSelectServersButton;
    }

    template<>
    QnSelectResourcesButton* createSelectButton<vms::rules::SourceUserField>()
    {
        return new QnSelectUsersButton;
    }

    template<typename T>
    void setIds(const QnUuidList& ids);

    template<>
    void setIds<vms::rules::SourceCameraField>(const QnUuidList& ids)
    {
        auto resourcePool = qnClientCoreModule->commonModule()->resourcePool();

        auto resourceList = resourcePool->getResourcesByIds<QnVirtualCameraResource>(ids);

        auto selectDeviceButtonPtr = static_cast<QnSelectDevicesButton*>(selectResourceButton);
        selectDeviceButtonPtr->selectDevices(resourceList);

        if (resourceList.isEmpty())
        {
            const QnCameraDeviceStringSet deviceStringSet{
                tr("Select at least one Device"),
                tr("Select at least one Camera"),
                tr("Select at least one I/O module")
            };

            const auto allCameras = resourcePool->getAllCameras();
            const auto deviceType
                = QnDeviceDependentStrings::calculateDeviceType(resourcePool, allCameras);

            alertLabel->setText(deviceStringSet.getString(deviceType));
            alertLabel->setVisible(true);
        }
        else
        {
            alertLabel->setVisible(false);
        }
    }

    template<>
    void setIds<vms::rules::SourceServerField>(const QnUuidList& ids)
    {
        auto resourcePool = qnClientCoreModule->commonModule()->resourcePool();

        auto resourceList = resourcePool->getResourcesByIds<QnMediaServerResource>(ids);

        auto selectServersButtonPtr = static_cast<QnSelectServersButton*>(selectResourceButton);
        selectServersButtonPtr->selectServers(
            resourcePool->getResourcesByIds<QnMediaServerResource>(ids));

        if (resourceList.isEmpty())
        {
            alertLabel->setText(tr("Select at least one Server"));
            alertLabel->setVisible(true);
        }
        else
        {
            alertLabel->setVisible(false);
        }
    }

    template<>
    void setIds<vms::rules::SourceUserField>(const QnUuidList& ids)
    {
        auto resourcePool = qnClientCoreModule->commonModule()->resourcePool();

        auto resourceList = resourcePool->getResourcesByIds<QnUserResource>(ids);

        auto selectUsersButtonPtr = static_cast<QnSelectUsersButton*>(selectResourceButton);
        selectUsersButtonPtr->selectUsers(
            resourcePool->getResourcesByIds<QnUserResource>(ids));

        if (resourceList.isEmpty())
        {
            alertLabel->setText(tr("Select at least one User"));
            alertLabel->setVisible(true);
        }
        else
        {
            alertLabel->setVisible(false);
        }
    }

};

} // namespace nx::vms::client::desktop::rules
