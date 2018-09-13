#pragma once

#include <nx/client/desktop/common/utils/abstract_text_provider.h>

namespace nx::client::desktop {

class LicenseUsageProvider: public AbstractTextProvider
{
    Q_OBJECT

public:
    using AbstractTextProvider::AbstractTextProvider; //< Forward constructors.

    virtual bool limitExceeded() const = 0;

signals:
    void stateChanged();
};

} // namespace nx::client::desktop
