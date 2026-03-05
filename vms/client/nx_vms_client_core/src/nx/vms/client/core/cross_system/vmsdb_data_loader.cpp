// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "vmsdb_data_loader.h"

#include <chrono>
#include <optional>

#include <QtCore/QElapsedTimer>
#include <QtCore/QTimer>

#include <nx/analytics/properties.h>
#include <nx/reflect/json.h>
#include <nx/utils/json/qt_containers_reflect.h>
#include <nx/utils/log/log.h>
#include <nx/vms/client/core/network/vmsdb_connection.h>
#include <nx/vms/client/core/system_context.h>

namespace nx::vms::client::core {

using namespace std::chrono;
using namespace nx::vms::api;
using HttpHeaders = nx::network::http::HttpHeaders;

struct VmsDbDataLoader::Private
{
    VmsDbDataLoader* const q;
    VmsDbConnection* const connection;
    nx::Uuid organizationId;
    bool initialDataLoaded = false;

    std::optional<DeviceModelList> devices;

    nx::network::rest::Params params(nx::network::rest::Params extra)
    {
        extra.insert("organizationId", organizationId.toSimpleString());

        return extra;
    }

    void requestDevices()
    {
        NX_VERBOSE(this, "Updating devices");
        auto callback =
            [this](::rest::Handle, rest::ErrorOrData<withOrgV<DeviceModelV4>> data)
            {
                if (!data)
                {
                    NX_WARNING(this, "Devices request failed: %1", data.error().errorId);
                    return;
                }

                NX_VERBOSE(this, "Received %1 cameras", data->size());
                devices = std::move(*data);

                updateStatus();
            };

        connection->get<withOrgV<DeviceModelV4>>(
            "/vmsDb/v4/devices",
            params({{"_with", "id,siteId,name,status,parameters.customGroupId"}}),
            std::move(callback),
            q);
    }

    void requestData(nx::Uuid organizationId)
    {
        this->organizationId = organizationId;

        if (!devices)
            requestDevices();

        updateStatus();
    }

    bool isDataNeeded() const
    {
        return !devices;
    }

    void updateStatus()
    {
        if (initialDataLoaded || isDataNeeded())
            return;

        emit q->ready();
        initialDataLoaded = true;
    }
};

VmsDbDataLoader::VmsDbDataLoader(VmsDbConnection* connection,  QObject* parent):
    QObject(parent),
    d(new Private{
        .q = this,
        .connection = connection,
    })
{
}

VmsDbDataLoader::~VmsDbDataLoader()
{
}

void VmsDbDataLoader::start(nx::Uuid organizationId)
{
    d->requestData(organizationId);
}

bool VmsDbDataLoader::isReady() const
{
    return !d->isDataNeeded();
}

VmsDbDataLoader::DeviceModelList VmsDbDataLoader::devices() const
{
    return d->devices.value_or(DeviceModelList());
}

} // namespace nx::vms::client::core
