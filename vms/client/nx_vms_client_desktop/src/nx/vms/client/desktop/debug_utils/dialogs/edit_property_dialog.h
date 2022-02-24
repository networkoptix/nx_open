// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <ui/dialogs/common/button_box_dialog.h>

#include <nx/utils/impl_ptr.h>

namespace nx::vms::client::desktop {

/**
 * A dialog to enter property name and value strings.
 */
class EditPropertyDialog: public QnButtonBoxDialog
{
    Q_OBJECT
    using base_type = QnButtonBoxDialog;

public:
    EditPropertyDialog(QWidget* parent = nullptr);
    virtual ~EditPropertyDialog() override;

    QString propertyName() const;
    QString propertyValue() const;

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::desktop
