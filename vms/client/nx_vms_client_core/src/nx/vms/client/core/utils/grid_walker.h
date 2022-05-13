// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QRect>

namespace nx::vms::client::core {

/**
 * Walk around the given grid, starting from the top left corner. Default state is invalid until
 * next() is called.
 */
struct NX_VMS_CLIENT_CORE_API GridWalker
{
    enum class Policy
    {
        Sequential,
        Snake,
        Round,
    };

    GridWalker(const QRect& grid, Policy policy = Policy::Sequential);

    QPoint pos() const;

    /**
     * Go to the next cell.
     * @return true if current state is valid.
     */
    bool next();

    int x;
    int y;
    const QRect grid;
    const Policy policy;
};

} // namespace nx::vms::client::core
