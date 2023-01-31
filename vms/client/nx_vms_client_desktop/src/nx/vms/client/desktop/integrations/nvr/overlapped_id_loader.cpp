// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "overlapped_id_loader.h"

#include <chrono>

#include <api/server_rest_connection.h>
#include <nx/utils/guarded_callback.h>

#include "overlapped_id_store.h"

using namespace std::chrono_literals;

namespace {

static constexpr auto kLoadingAttemptTimeout = 1s;

} // namespace

namespace nx::vms::client::desktop::integrations {

OverlappedIdLoader::OverlappedIdLoader(
    SystemContext* systemContext,
    const QString& groupId,
    std::shared_ptr<OverlappedIdStore> store,
    QObject* parent)
    :
    QObject(parent),
    SystemContextAware(systemContext),
    m_groupId(groupId),
    m_store(store)
{
    m_loadingAttemptTimer.setSingleShot(true);
    m_loadingAttemptTimer.setInterval(kLoadingAttemptTimeout);

    connect(&m_loadingAttemptTimer, &QTimer::timeout, this, &OverlappedIdLoader::requestIdList);
}

OverlappedIdLoader::~OverlappedIdLoader()
{
    stop();
}

void OverlappedIdLoader::start()
{
    m_stopped = false;
    requestIdList();
}

void OverlappedIdLoader::stop()
{
    m_loadingAttemptTimer.stop();
    m_stopped = true;
}

void OverlappedIdLoader::requestIdList()
{
    auto callback = nx::utils::guarded(this,
        [this](
            bool success,
            rest::Handle /*requestId*/,
            nx::vms::api::OverlappedIdResponse result)
        {
            if (!success)
            {
                NX_WARNING(this, "Received invalid overlapped id data.");
                if (!m_stopped)
                    m_loadingAttemptTimer.start();
                return;
            }

            m_store->setCurrentId(result.currentOverlappedId);
            m_store->setIdList(result.availableOverlappedIds);
        });

    auto connection = connectedServerApi();
    if (!connection)
        return;

    connection->getOverlappedIds(m_groupId, callback, thread());
}

} // namespace nx::vms::client::desktop::integrations
