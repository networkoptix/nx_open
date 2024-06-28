// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/utils/impl_ptr.h>

#include "../abstract_system_finder.h"
#include "cloud_system_description.h"

namespace nx::vms::client::core {

class CloudSystemFinder: public AbstractSystemFinder
{
    Q_OBJECT
    typedef AbstractSystemFinder base_type;

public:
    CloudSystemFinder(QObject* parent = nullptr);
    virtual ~CloudSystemFinder() override;

    SystemDescriptionList systems() const override;
    SystemDescriptionPtr getSystem(const QString& id) const override;

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::core
