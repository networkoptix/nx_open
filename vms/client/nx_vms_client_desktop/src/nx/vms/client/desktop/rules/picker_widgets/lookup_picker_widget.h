// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/vms/rules/event_filter_fields/lookup_field.h>

#include "field_picker_widget.h"

class QComboBox;
class QStackedWidget;
class QLineEdit;

namespace nx::vms::client::desktop::rules {

class LookupPicker: public TitledFieldPickerWidget<vms::rules::LookupField>
{
    Q_OBJECT

public:
    LookupPicker(QnWorkbenchContext* context, CommonParamsWidget* parent);

private:
    QComboBox* m_checkTypeComboBox{nullptr};
    QComboBox* m_checkSourceComboBox{nullptr};

    QStackedWidget* m_stackedWidget{nullptr};
    QLineEdit* m_lineEdit{nullptr};
    QComboBox* m_lookupListComboBox{nullptr};

    void updateUi() override;
};

} // namespace nx::vms::client::desktop::rules
