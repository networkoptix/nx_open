// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "dummy.h"

namespace nx::vms::client::desktop::figure {

FigurePtr Dummy::create()
{
    return FigurePtr(new Dummy());
}

Dummy::Dummy():
    base_type(FigureType::dummy, {}, QColor(), QRectF(), core::StandardRotation::rotate0)
{
};

bool Dummy::isValid() const
{
    return true;
}

FigurePtr Dummy::clone() const
{
    return create();
}

} // namespace nx::vms::client::desktop::figure
