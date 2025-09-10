// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <nx/reflect/enum_instrument.h>
#include <nx/utils/impl_ptr.h>

namespace nx::vms::client::core {

NX_REFLECTION_ENUM_CLASS(CloudService,
    none = 0,
    cloud_notifications = 1 << 0,
    push_notifications = 1 << 1,
    docdb = 1 << 2
);

class NX_VMS_CLIENT_CORE_API CloudServiceChecker: public QObject
{
    Q_OBJECT

public:
    Q_DECLARE_FLAGS(CloudServices, CloudService)
    explicit CloudServiceChecker(QObject* parent = nullptr);
    virtual ~CloudServiceChecker() override;

    bool hasService(const CloudService& service) const;

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::core
