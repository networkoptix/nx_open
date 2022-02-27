// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/vms/client/desktop/common/utils/abstract_text_provider.h>

namespace nx::vms::client::desktop {

class LicenseUsageProvider: public AbstractTextProvider
{
    Q_OBJECT

public:
    using AbstractTextProvider::AbstractTextProvider; //< Forward constructors.

    virtual bool limitExceeded() const = 0;

signals:
    void stateChanged();
};

} // namespace nx::vms::client::desktop
