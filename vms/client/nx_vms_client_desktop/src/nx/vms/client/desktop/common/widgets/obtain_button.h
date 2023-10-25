// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/utils/impl_ptr.h>
#include <nx/vms/client/desktop/common/widgets/button_with_confirmation.h>

namespace nx::vms::client::desktop {

// A flat button that shows an animation of waiting (obtaining) data
class ObtainButton: public ButtonWithConfirmation
{
    Q_OBJECT
    using base_type = ButtonWithConfirmation;

public:
    explicit ObtainButton(const QString& text, QWidget* parent = nullptr);
    virtual ~ObtainButton() override;

    virtual void setVisible(bool visible) override;

protected:
    virtual void paintEvent(QPaintEvent* event) override;

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::desktop
