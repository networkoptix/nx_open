// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <core/resource/layout_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/vms/client/core/skin/skin.h>
#include <nx/vms/client/desktop/resource_dialogs/multiple_layout_selection_dialog.h>
#include <nx/vms/client/desktop/style/resource_icon_cache.h>
#include <nx/vms/rules/action_builder_fields/target_layout_field.h>
#include <ui/widgets/select_resources_button.h>

#include "resource_picker_widget_base.h"

namespace nx::vms::client::desktop::rules {

class TargetLayoutPicker:
    public ResourcePickerWidgetBase<vms::rules::TargetLayoutField>
{
    Q_OBJECT
public:
    using ResourcePickerWidgetBase<vms::rules::TargetLayoutField>::ResourcePickerWidgetBase;

protected:
    void onSelectButtonClicked() override
    {
        auto field = theField();
        auto selectedLayouts = field->value();

        if (!MultipleLayoutSelectionDialog::selectLayouts(selectedLayouts, this))
            return;

        field->setValue(selectedLayouts);
    }

    void updateUi() override
    {
        const auto layouts = resourcePool()->getResourcesByIds<QnLayoutResource>(theField()->value());

        m_selectButton->setForegroundRole(
            !layouts.empty() ? QPalette::BrightText : QPalette::ButtonText);

        m_selectButton->setText(buttonText(layouts));
        m_selectButton->setIcon(buttonIcon(layouts));
    }

    QString buttonText(const QnSharedResourcePointerList<QnLayoutResource>& layouts) const
    {
        if (layouts.empty())
            return tr("Select layout...");

        if (layouts.size() == 1)
            return layouts.first()->getName();

        return tr("%n layouts", "", layouts.size());
    }

    QIcon buttonIcon(const QnSharedResourcePointerList<QnLayoutResource>& layouts) const
    {
        if (layouts.empty())
            return qnSkin->icon("tree/buggy.png");

        return core::Skin::maximumSizePixmap(
            layouts.size() > 1
                ? qnResIconCache->icon(QnResourceIconCache::Layouts)
                : qnResIconCache->icon(layouts.first()),
            QIcon::Selected,
            QIcon::Off,
            /*correctDevicePixelRatio*/ false);
    }
};

} // namespace nx::vms::client::desktop::rules
