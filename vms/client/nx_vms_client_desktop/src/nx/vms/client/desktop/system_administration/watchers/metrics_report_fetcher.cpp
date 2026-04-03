// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "metrics_report_fetcher.h"

#include <api/server_rest_connection.h>
#include <nx/utils/guarded_callback.h>
#include <nx/utils/scoped_change_notifier.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/common/system_settings.h>

namespace {

static constexpr auto kManifestEndpoint = "/rest/v4/metrics/manifest";
static constexpr auto kValuesEndpoint = "/rest/v4/metrics/values";
static constexpr auto kAlarmsEndpoint = "/rest/v4/metrics/alarms";

QVector<QString> endpoints()
{
    return {
        kManifestEndpoint,
        kValuesEndpoint,
        kAlarmsEndpoint};
}

} // namespace

namespace nx::vms::client::desktop {

struct MetricsReportFetcher::Private
{
    MetricsReportFetcher* q;

    void checkResult();

    QSet<rest::Handle> runningRequests;
    QHash<QString, QByteArray> replies;
};

void MetricsReportFetcher::Private::checkResult()
{
    if (q->isRunning())
        return;

    QJsonObject replyObject;
    for (const auto& endpoint: endpoints())
    {
        if (replies.contains(endpoint))
            replyObject.insert(endpoint, QJsonValue::fromJson(replies.take(endpoint)));
    }

    const QJsonObject result{
        {"reply", replyObject},
        {"time", QDateTime::currentDateTime().toString(Qt::ISODate)},
        {"system", q->systemSettings()->systemName()}};

    emit q->reportReady(QJsonDocument(result));
    emit q->isRunningChanged();
}

MetricsReportFetcher::MetricsReportFetcher(SystemContext* context, QObject* parent):
    base_type(parent),
    SystemContextAware(context),
    d(new Private(this))
{
}

MetricsReportFetcher::~MetricsReportFetcher()
{
    cancelRequest();
}

void MetricsReportFetcher::requestMetricsReport()
{
    if (!NX_ASSERT(!isRunning(), "Another request is already running"))
        return;

    const auto api = systemContext()->connectedServerApi();
    if (!api)
        return;

    nx::utils::ScopedChangeNotifier<bool> isRunningChangeNotifier(
        [this]{ return isRunning(); },
        [this]{ emit isRunningChanged(); });

    for (const auto& endpoint: endpoints())
    {
        auto callback = nx::utils::guarded(this,
            [this, endpoint](bool success,
                rest::Handle requestId,
                QByteArray result,
                auto /*headers*/)
            {
                if (!d->runningRequests.contains(requestId))
                    return;

                d->runningRequests.remove(requestId);
                if (success)
                    d->replies.insert(endpoint, result);

                d->checkResult();
            });

        const auto handle = api->getRawResult(
            endpoint,
            /*params*/ {},
            std::move(callback),
            thread());

        if (handle)
            d->runningRequests.insert(handle);
    }
}

void MetricsReportFetcher::cancelRequest()
{
    if (const auto api = systemContext()->connectedServerApi())
    {
        for (const auto& handle: d->runningRequests)
            api->cancelRequest(handle);
    }

    nx::utils::ScopedChangeNotifier<bool> isRunningChangeNotifier(
        [this]{ return isRunning(); },
        [this]{ emit isRunningChanged(); });

    d->runningRequests.clear();
    d->replies.clear();
}

bool MetricsReportFetcher::isRunning() const
{
    return !d->runningRequests.empty();
}

} // namespace nx::vms::client::desktop
