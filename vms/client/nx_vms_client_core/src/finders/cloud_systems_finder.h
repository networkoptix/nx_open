// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <network/cloud_system_description.h>
#include <nx/utils/impl_ptr.h>
#include <nx/utils/thread/mutex.h>
#include <nx/vms/client/core/network/cloud_status_watcher.h>

#include "abstract_systems_finder.h"

namespace nx::vms::client::core {

class CloudSystemsFinder: public QnAbstractSystemsFinder
{
    Q_OBJECT
    typedef QnAbstractSystemsFinder base_type;

public:
    CloudSystemsFinder(QObject* parent = nullptr);
    virtual ~CloudSystemsFinder() override;

    SystemDescriptionList systems() const override;
    QnSystemDescriptionPtr getSystem(const QString& id) const override;

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::core
