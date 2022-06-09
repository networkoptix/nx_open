// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "abstract_enumeration.h"

#include <nx/utils/impl_ptr.h>

namespace nx::analytics::taxonomy { class AbstractEnumType; }

namespace nx::vms::client::desktop::analytics::taxonomy {

class Enumeration: public AbstractEnumeration
{
public:
    Enumeration(QObject* parent = nullptr);

    virtual ~Enumeration() override;

    virtual std::vector<QString> items() const override;

    void addEnumType(nx::analytics::taxonomy::AbstractEnumType* enumType);

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::desktop::analytics::taxonomy
