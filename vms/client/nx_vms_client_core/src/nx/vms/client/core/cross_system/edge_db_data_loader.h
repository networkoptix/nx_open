// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <map>
#include <vector>

#include <QtCore/QObject>

#include <nx/network/rest/result.h>
#include <nx/utils/impl_ptr.h>
#include <nx/vms/api/data/device_model.h>
#include <nx/vms/api/data/layout_data.h>
#include <nx/vms/api/types/access_rights_types.h>

namespace nx::vms::client::core {

class EdgeDbConnection;

/** Mixin to add mandatory edgeDb data fields to existing API models. */
template<typename ModelType>
struct WithSite: public ModelType
{
    using Model = ModelType;

    QString siteId;
};

template<typename Model>
using WithSiteList = std::vector<WithSite<Model>>;

#define WithSite_Fields (siteId)
NX_REFLECTION_INSTRUMENT(WithSite<nx::vms::api::DeviceModelV4>,
    DeviceModelV4_Fields WithSite_Fields);
NX_REFLECTION_INSTRUMENT(WithSite<nx::vms::api::LayoutData>, LayoutData_Fields WithSite_Fields);

/**
 * Loads from edgeDb the Organization data required to show resources of all Organization Sites
 * without connecting to them.
 *
 * Three collections are requested in parallel:
 * - permissions of the current User by Site id, its keys are the Sites available to the User;
 * - Devices and Layouts of all Sites, from the Organization lists with only the fields used by
 *   the client. The lists are paged: each page has a `_next` cursor to request the next one.
 *
 * Emits either `ready()` when the full dataset is loaded, or `failed()` on the first error. In
 * the latter case no data is kept. `start()` may be called again to reload the data; responses
 * of the previous run are discarded.
 *
 * Owns the loaded data: its users, like `EdgeDbDataProcessor`, read it from here and keep no
 * copies.
 */
class NX_VMS_CLIENT_CORE_API EdgeDbDataLoader: public QObject
{
    Q_OBJECT

public:
    using DeviceModelList = WithSiteList<nx::vms::api::DeviceModelV4>;
    using LayoutDataList = WithSiteList<nx::vms::api::LayoutData>;

    /** Current User permissions by Site id. */
    using PermissionsMap = std::map<QString, nx::vms::api::PermissionsModel>;

    struct SiteData
    {
        DeviceModelList devices;
        LayoutDataList layouts;
    };

    /** Devices and Layouts by Site id. */
    using SitesMap = std::map<QString, SiteData>;

public:
    EdgeDbDataLoader(EdgeDbConnection* connection, QObject* parent = nullptr);
    virtual ~EdgeDbDataLoader() override;

    void start(nx::Uuid organizationId);

    bool isReady() const;

    // The data is empty until the loader is ready.
    const PermissionsMap& permissions() const;
    const SitesMap& sites() const;

signals:
    void ready();
    void failed(const nx::network::rest::Result& error);

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::core
