// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <ui/dialogs/common/button_box_dialog.h>

#include <nx/utils/impl_ptr.h>

namespace nx::vms::client::desktop {

/**
 * A dialog to choose, load and display simple QML components.
 */
class QmlTestDialog: public QnButtonBoxDialog
{
    Q_OBJECT
    using base_type = QnButtonBoxDialog;

public:
    QmlTestDialog(QWidget* parent = nullptr);
    virtual ~QmlTestDialog() override;

    static void registerAction();
private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::desktop
