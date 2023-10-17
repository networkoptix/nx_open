// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/vms/rules/event_filter_fields/object_lookup_field.h>

#include "field_picker_widget.h"

class QComboBox;
class QStackedWidget;
class QLineEdit;

namespace nx::vms::client::desktop::rules {

class LookupListsModel;

class ObjectLookupPicker: public TitledFieldPickerWidget<vms::rules::ObjectLookupField>
{
    Q_OBJECT

public:
    ObjectLookupPicker(SystemContext* context, CommonParamsWidget* parent);

protected:
    void onDescriptorSet() override;

private:
    QComboBox* m_checkTypeComboBox{nullptr};
    QStackedWidget* m_stackedWidget{nullptr};
    QLineEdit* m_lineEdit{nullptr};
    QComboBox* m_lookupListComboBox{nullptr};
    LookupListsModel* m_lookupListsModel{nullptr};

    void updateUi() override;
};

} // namespace nx::vms::client::desktop::rules
