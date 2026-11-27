// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "edge_db_data_loader.h"

#include <functional>
#include <iterator>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <utility>

#include <nx/reflect/instrument.h>
#include <nx/reflect/json.h>
#include <nx/utils/json/qt_containers_reflect.h>
#include <nx/utils/log/log.h>
#include <nx/vms/client/core/network/edge_db_connection.h>

namespace nx::vms::client::core {

using namespace nx::vms::api;

namespace {

/** Page of an edgeDb records list. */
template<typename Model>
struct EdgeDbPage
{
    std::vector<Model> results;

    /** Cursor to request the next page with, empty on the last page. */
    std::string _next;
};

NX_REFLECTION_INSTRUMENT_TEMPLATE(EdgeDbPage, (results) (_next))

constexpr auto kCollectionPath = "/edgeDb/v4/organizations/%1/%2";

const QString kPermissions = "permissions";
const QString kDevices = "devices";
const QString kLayouts = "layouts";

const QString kNextParam = "_next";
const QString kWithParam = "_with";

// Lists accept at most 10 fields and no whole `parameters`. Type defines the Device kind, like a
// desktop or virtual camera. Custom groups are required by the resource tree folders and its
// search. Device status is not requested: the status of a Device of a not connected Site follows
// the Site context state.
const QString kDeviceListFields = "id,siteId,name,typeId,group,parameters.customGroupId";

// Only the fields used by the resource tree. Items and the Layout's own fields, like the cell size
// or `locked`, keep their defaults.
const QString kLayoutListFields = "id,siteId,name,parentId";

} // namespace

struct EdgeDbDataLoader::Private
{
    template<typename Model>
    using Handler = std::function<void(WithSiteList<Model>)>;

    template<typename Model>
    using ResponseHandler = std::function<void(rest::ErrorOrData<Model>)>;

    EdgeDbDataLoader* const q;
    EdgeDbConnection* const connection;

    nx::Uuid organizationId;

    /**
     * Set by the first error of the run: no more requests are sent and only this error is
     * reported. Responses of the running requests are ignored after it anyway, so it matters when
     * the requests are not sent at all, e.g. when the Cloud is not online.
     */
    bool hasFailed = false;

    /**
     * Only responses of these requests are handled. Canceled requests, including the ones of the
     * previous runs, are removed from here.
     */
    std::set<rest::Handle> runningRequests;

    /** Collections of the running load. Each one is set when it is fully loaded. */
    struct Loading
    {
        std::optional<PermissionsMap> permissions;
        std::optional<DeviceModelList> devices;
        std::optional<LayoutDataList> layouts;
    };
    Loading loading;

    /** Data of the finished load, empty until all the collections are loaded. */
    PermissionsMap permissions;
    SitesMap sites;
    bool isReady = false;

    QString path(const QString& collection) const
    {
        return nx::format(kCollectionPath, organizationId.toSimpleString(), collection);
    }

    void clear()
    {
        loading = {};
        permissions.clear();
        sites.clear();
        isReady = false;
    }

    void cancelRequests()
    {
        // Queued requests have greater handles. Canceling them first keeps a canceled running
        // request from starting the next queued one.
        const auto handles = std::exchange(runningRequests, {});
        for (auto it = handles.rbegin(); it != handles.rend(); ++it)
            connection->cancel(*it);
    }

    void start(nx::Uuid organizationId)
    {
        cancelRequests();
        this->organizationId = organizationId;
        hasFailed = false;
        clear();

        NX_DEBUG(this, "Loading data of the Organization %1", organizationId);

        requestPermissions();
        requestDevices();
        requestLayouts();
    }

    /** Sends the request and calls the handler only if the request is still running. */
    template<typename Model>
    void get(const QString& collection,
        const nx::network::rest::Params& params,
        ResponseHandler<Model> handler)
    {
        if (hasFailed)
            return;

        auto callback = [this, handler = std::move(handler)](
                            rest::Handle handle, rest::ErrorOrData<Model> data)
        {
            if (runningRequests.erase(handle) > 0)
                handler(std::move(data));
        };

        const auto handle =
            connection->get<Model>(path(collection), params, std::move(callback), q);
        if (!handle)
        {
            fail(collection, nx::network::rest::Result::serviceUnavailable("Request is not sent"));
            return;
        }

        runningRequests.insert(handle);
    }

    void requestPermissions()
    {
        get<PermissionsMap>(kPermissions,
            /*params*/ {},
            [this](rest::ErrorOrData<PermissionsMap> data)
            {
                if (!data)
                {
                    fail(kPermissions, data.error());
                    return;
                }

                NX_VERBOSE(this, "Received permissions for %1 Sites", data->size());
                loading.permissions = std::move(*data);
                updateStatus();
            });
    }

    void requestDevices()
    {
        requestList<DeviceModelV4>(kDevices,
            kDeviceListFields,
            [this](DeviceModelList list)
            {
                loading.devices = std::move(list);
                updateStatus();
            });
    }

    void requestLayouts()
    {
        requestList<LayoutData>(kLayouts,
            kLayoutListFields,
            [this](LayoutDataList list)
            {
                loading.layouts = std::move(list);
                updateStatus();
            });
    }

    /**
     * Requests all pages of the collection one by one. A page is the response with a portion of
     * the records and the cursor of the next page.
     * @param loaded Records of the previous pages.
     * @param cursor `_next` of the previous page, which points after its last record. Empty for
     *     the first page.
     */
    template<typename Model>
    void requestList(const QString& collection,
        const QString& fields,
        Handler<Model> handler,
        std::shared_ptr<WithSiteList<Model>> loaded = {},
        const std::string& cursor = {})
    {
        if (!loaded)
            loaded = std::make_shared<WithSiteList<Model>>();

        auto callback = [this, collection, fields, handler = std::move(handler), loaded](
                            rest::ErrorOrData<EdgeDbPage<WithSite<Model>>> page)
        {
            if (!page)
            {
                fail(collection, page.error());
                return;
            }

            NX_VERBOSE(this, "Received %1 %2", page->results.size(), collection);
            loaded->insert(loaded->end(),
                std::make_move_iterator(page->results.begin()),
                std::make_move_iterator(page->results.end()));

            if (page->_next.empty())
            {
                handler(std::move(*loaded));
                return;
            }

            requestList<Model>(collection, fields, handler, loaded, page->_next);
        };

        nx::network::rest::Params params{{kWithParam, fields}};
        if (!cursor.empty())
            params.insert(kNextParam, QString::fromStdString(cursor));

        get<EdgeDbPage<WithSite<Model>>>(collection, params, std::move(callback));
    }

    void fail(const QString& collection, const nx::network::rest::Result& error)
    {
        NX_WARNING(
            this, "Request of %1 failed: %2, %3", collection, error.errorId, error.errorString);

        cancelRequests();
        hasFailed = true;
        clear();
        emit q->failed(error);
    }

    /** Makes the data available when all the collections are loaded. */
    void updateStatus()
    {
        if (!loading.permissions || !loading.devices || !loading.layouts)
            return;

        NX_DEBUG(this,
            "Organization data loaded: %1 Sites, %2 Devices, %3 Layouts",
            loading.permissions->size(),
            loading.devices->size(),
            loading.layouts->size());

        permissions = std::move(*loading.permissions);
        for (auto& device: *loading.devices)
            sites[device.siteId].devices.push_back(std::move(device));
        for (auto& layout: *loading.layouts)
            sites[layout.siteId].layouts.push_back(std::move(layout));
        loading = {};
        isReady = true;

        emit q->ready();
    }
};

EdgeDbDataLoader::EdgeDbDataLoader(EdgeDbConnection* connection, QObject* parent):
    QObject(parent),
    d(new Private{
        .q = this,
        .connection = connection,
    })
{
}

EdgeDbDataLoader::~EdgeDbDataLoader()
{
    d->cancelRequests();
}

void EdgeDbDataLoader::start(nx::Uuid organizationId)
{
    d->start(organizationId);
}

bool EdgeDbDataLoader::isReady() const
{
    return d->isReady;
}

const EdgeDbDataLoader::PermissionsMap& EdgeDbDataLoader::permissions() const
{
    return d->permissions;
}

const EdgeDbDataLoader::SitesMap& EdgeDbDataLoader::sites() const
{
    return d->sites;
}

} // namespace nx::vms::client::core
