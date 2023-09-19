// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "figure.h"

namespace nx::vms::client::desktop::figure {

/** Represents invisible dummy figure. */
class Dummy: public Figure
{
    using base_type = Figure;

public:
    static FigurePtr create();

    virtual bool isValid() const override;
    virtual FigurePtr clone() const override;

private:
    Dummy();
};

} // namespace nx::vms::client::desktop::figure
