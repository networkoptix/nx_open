// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "click_instrument.h"

/**
 * This instrument listens to click events and sends "start object tracking" command to the server,
 * when it's needed.
 */
class ObjectTrackingInstrument: public ClickInstrument
{
    Q_OBJECT
    using base_type = ClickInstrument;

public:
    ObjectTrackingInstrument(QObject* parent = nullptr);
    virtual ~ObjectTrackingInstrument() override;
};
