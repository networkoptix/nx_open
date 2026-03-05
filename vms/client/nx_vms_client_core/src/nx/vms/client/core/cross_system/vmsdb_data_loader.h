// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <nx/utils/impl_ptr.h>
#include <nx/vms/api/data/device_model.h>

namespace nx::vms::client::core {

class VmsDbConnection;

/* Mixin to add mandatory VMSDB data fields to existing API models. */
template<typename ModelType>
struct withOrg: public ModelType
{
    using Model = ModelType;

    nx::Uuid organizationId;
    QString siteId;
};

template<typename Model>
using withOrgV = std::vector<withOrg<Model>>;

#define WithOrgFields (organizationId)(siteId)
NX_REFLECTION_INSTRUMENT(withOrg<nx::vms::api::DeviceModelV4>, DeviceModelV4_Fields WithOrgFields);

/**
 * Loads full set of data required for the cross-system cameras to work.
 *
 * Does not require additional control (create-and-forget). Emits `ready()` on first full dataset,
 * then `camerasUpdated()` when cameras are updated.
 */
class NX_VMS_CLIENT_CORE_API VmsDbDataLoader: public QObject
{
    Q_OBJECT

public:
    using DeviceModelList = withOrgV<nx::vms::api::DeviceModelV4>;

public:
    VmsDbDataLoader(VmsDbConnection* connection, QObject* parent = nullptr);
    virtual ~VmsDbDataLoader() override;

    void start(nx::Uuid organizationId);

    bool isReady() const;

    DeviceModelList devices() const;

signals:
    void ready();
    void resourceRemoved(nx::Uuid id);

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::core
