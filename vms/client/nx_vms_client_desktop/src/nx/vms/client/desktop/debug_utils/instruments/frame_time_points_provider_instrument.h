// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <vector>

#include <nx/utils/impl_ptr.h>
#include <ui/graphics/instruments/instrument.h>

namespace nx::vms::client::desktop {

/** Instrument that provides frame timestamps. Used in Functional Tests for the FPS measurement. */
class FrameTimePointsProviderInstrument: public Instrument
{
    Q_OBJECT
    using base_type = Instrument;

public:
    explicit FrameTimePointsProviderInstrument(QObject* parent = nullptr);
    virtual ~FrameTimePointsProviderInstrument() override;

     /**
     * @return Vector containing timestamps of recent viewport paint events. Measured in
     *     milliseconds elapsed since workbench initialization. Number of time points returned are
     *     limited by the <tt>storeFrameTimePoints</tt> ini parameter.
     */
    std::vector<qint64> getFrameTimePoints();

protected:
    virtual bool paintEvent(QWidget* viewport, QPaintEvent* event) override;

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // nx::vms::client::desktop
