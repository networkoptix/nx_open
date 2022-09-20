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

    QStringList selectedObjectTypeIds() const;
    void setSelectedObjectTypeIds(const QStringList& objectTypeIds);
    QString selectedMainObjectTypeId() const;
    void setSelectedMainObjectTypeId(const QString& objectTypeId);
};

} // namespace nx::vms::client::desktop
