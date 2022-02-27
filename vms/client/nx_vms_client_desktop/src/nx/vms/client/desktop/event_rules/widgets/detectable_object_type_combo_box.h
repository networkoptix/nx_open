// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <ui/widgets/common/tree_combo_box.h>

namespace nx::vms::client::desktop {

/**
 * A combo box to select an analytics detectable object type from those available in the system.
 */
class DetectableObjectTypeComboBox: public QnTreeComboBox
{
    Q_OBJECT
    using base_type = QnTreeComboBox;

public:
    DetectableObjectTypeComboBox(QWidget* parent = nullptr);

    QString selectedObjectTypeId() const;
    void setSelectedObjectTypeId(const QString& value);
};

} // namespace nx::vms::client::desktop
