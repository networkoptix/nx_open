// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/vms/client/desktop/common/widgets/hover_button.h>

namespace nx::vms::client::desktop {

/**
 * X button
 */
class CloseButton: public HoverButton
{
    Q_OBJECT
    using base_type = HoverButton;

public:
    CloseButton(QWidget* parent = nullptr);
    void setAccented(bool accented);

protected:
    virtual void paintEvent(QPaintEvent* event) override;

private:
    bool m_accented = false;
};

} // namespace nx::vms::client::desktop
