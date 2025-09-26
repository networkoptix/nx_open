// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QScopedPointer>

#include "jump_to_live_button.h"

namespace nx::vms::client::desktop {

class SoftTriggerButtonPrivate;

class SoftTriggerButton: public JumpToLiveButton
{
    Q_OBJECT
    using base_type = JumpToLiveButton;

public:
    SoftTriggerButton(QGraphicsItem* parent = nullptr);
    virtual ~SoftTriggerButton();

    virtual void paint(QPainter* painter, const QStyleOptionGraphicsItem* option,
        QWidget* widget) override;

private:
    Q_DECLARE_PRIVATE(SoftTriggerButton)
    QScopedPointer<SoftTriggerButtonPrivate> d_ptr;
};

} // namespace nx::vms::client::desktop
