// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtWidgets/QComboBox>
#include <QtWidgets/QPushButton>

#include <nx/vms/rules/action_builder_fields/sound_field.h>

#include "field_picker_widget.h"

namespace nx::vms::client::desktop { class ServerNotificationCache; }

namespace nx::vms::client::desktop::rules {

class SoundPicker: public PlainFieldPickerWidget<vms::rules::SoundField>
{
    Q_OBJECT

public:
    SoundPicker(WindowContext* context, ParamsWidget* parent);

    void updateUi() override;

private:
    ServerNotificationCache* m_serverNotificationCache{nullptr};
    QComboBox* m_comboBox{nullptr};
    QPushButton* m_manageButton{nullptr};

    void onCurrentIndexChanged(int index);
    void onManageButtonClicked();
};

} // namespace nx::vms::client::desktop::rules
