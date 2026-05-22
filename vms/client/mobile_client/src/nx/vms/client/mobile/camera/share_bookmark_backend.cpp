// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "share_bookmark_backend.h"

#include <QtQml/QtQml>

#include <camera/camera_bookmarks_manager.h>
#include <core/resource/camera_bookmark.h>
#include <core/resource/camera_resource.h>
#include <core/resource/user_resource.h>
#include <core/resource_access/resource_access_manager.h>
#include <core/resource_access/resource_access_subject.h>
#include <mobile_client/mobile_client_settings.h>
#include <mobile_client/mobile_client_ui_controller.h>
#include <nx/network/cloud/cloud_connect_controller.h>
#include <nx/network/http/http_types.h>
#include <nx/network/socket_global.h>
#include <nx/network/url/url_builder.h>
#include <nx/utils/math/math.h>
#include <nx/utils/scoped_connections.h>
#include <nx/vms/api/data/bookmark_models.h>
#include <nx/vms/client/core/analytics/analytics_attribute_helper.h>
#include <nx/vms/client/core/client_core_globals.h>
#include <nx/vms/client/core/watchers/feature_access_watcher.h>
#include <nx/vms/client/core/watchers/server_time_watcher.h>
#include <nx/vms/client/core/watchers/user_watcher.h>
#include <nx/vms/client/mobile/system_context.h>
#include <nx/vms/client/mobile/system_context_accessor.h>
#include <nx/vms/client/mobile/ui/share_link_helper.h>
#include <nx/vms/client/mobile/ui/ui_controller.h>
#include <nx/vms/common/api/helpers/bookmark_api_converter.h>
#include <nx/vms/text/human_readable.h>
#include <utils/common/delayed.h>
#include <utils/common/synctime.h>

#include "details/bookmark_constants.h"

namespace nx::vms::client::mobile {

using namespace std::chrono;

struct ShareBookmarkBackend::Private: public SystemContextAccessor
{
    ShareBookmarkBackend* const q;
    QPointer<timeline::AbstractObjectData> objectData;
    utils::ScopedConnection objectDataConnection;

    bool isAvailable = false;
    QnCameraBookmarksManager::BookmarkOperation operation;
    common::CameraBookmark bookmark;

    Private(ShareBookmarkBackend* q);

    void shareBookmarkLink(const QString& bookmarkId) const;
    bool updateShareParams(const common::BookmarkShareableParams& shareParams,
        bool showNativeShareSheet = false);
};

ShareBookmarkBackend::Private::Private(ShareBookmarkBackend* q):
    q(q)
{
}

void ShareBookmarkBackend::Private::shareBookmarkLink(const QString& bookmarkFullId) const
{
    const auto context = systemContext();
    if (!NX_ASSERT(context))
        return;

    const auto systemId = context->cloudSystemId();
    const auto result = network::url::Builder()
        .setScheme(network::http::kSecureUrlSchemeName)
        .setHost(nx::network::SocketGlobals::cloud().cloudHost())
        .setPath(QString("share/%1/%2").arg(systemId, bookmarkFullId))
        .toUrl().toQUrl();

    NX_DEBUG(this, "Shared bookmark link: ", result.toString());
    shareLink(result);
}

bool ShareBookmarkBackend::Private::updateShareParams(
    const common::BookmarkShareableParams& shareParams,
    bool showNativeShareSheet)
{
    const auto context = systemContext();
    if (!NX_ASSERT(context))
        return false;

    const auto manager = context->cameraBookmarksManager();
    if (!NX_ASSERT(manager))
        return false;

    const auto handleResult =
        [this, backupShareParams = bookmark.share](const bool result)
        {
            if (result)
                return true;

            bookmark.share = backupShareParams;
            emit q->bookmarkChanged();

            executeLater([this]() { emit q->sharingFailed(); }, q);

            return false;
        };

    bookmark.share = shareParams;

    auto callback = nx::utils::guarded(q,
        [this, handleResult, showNativeShareSheet](
            bool result,
            const std::optional<api::BookmarkV3>& bookmarkV3)
        {
            if (!handleResult(result))
                return;

            if (operation == QnCameraBookmarksManager::BookmarkOperation::create)
                executeLater([this]() { emit q->bookmarkCreated(); }, q);

            // As we created or updated bookmark - there is no need to create it next time.
            operation = QnCameraBookmarksManager::BookmarkOperation::update;

            bookmark = common::bookmarkFromApi(bookmarkV3.value());

            if (showNativeShareSheet && bookmark.shareable() && NX_ASSERT(!bookmarkV3->id.isEmpty()))
                shareBookmarkLink(bookmarkV3->id);

            emit q->bookmarkChanged();
        });

    return handleResult(manager->submitBookmarkOperation(operation, bookmark, std::move(callback)));
}

//--------------------------------------------------------------------------------------------------

void ShareBookmarkBackend::registerQmlType()
{
    qmlRegisterType<ShareBookmarkBackend>("nx.vms.client.mobile", 1, 0, "ShareBookmarkBackend");

    qmlRegisterSingletonType<detail::BookmarkConstants>(
        "nx.vms.client.mobile", 1, 0, "BookmarkConstants",
        [](QQmlEngine*, QJSEngine*) -> QObject*
        {
            return new detail::BookmarkConstants();
        });
}

ShareBookmarkBackend::ShareBookmarkBackend(QObject* parent):
    base_type(parent),
    d{new Private(this)}
{
    const auto updateAvailablity =
        [this]()
        {
            const auto newValue =
                [this]()
                {
                    const auto camera = d->resource().dynamicCast<QnVirtualCameraResource>();
                    if (!camera)
                        return false;

                    const auto context = d->mobileSystemContext();
                    if (!NX_ASSERT(context))
                        return false;

                    const auto featureAccess = context->featureAccess();
                    if (!featureAccess)
                        return false;

                    const auto currentUser = context->userWatcher()->user();
                    const bool hasManageBookmarksPermission =
                        context->resourceAccessManager()->hasAccessRights(
                            currentUser, d->resource(), api::AccessRight::manageBookmarks);
                    return featureAccess->canUseShareBookmark()
                        && hasManageBookmarksPermission;
                }();

            if (d->isAvailable == newValue)
                return;

            d->isAvailable = newValue;
            emit isAvailableChanged();
        };

    updateAvailablity();

    connect(d.data(), &SystemContextAccessor::systemContextIsAboutToBeChanged, this,
        [this]()
        {
            const auto context = d->mobileSystemContext();
            if (!context)
                return;

            if (const auto access = context->featureAccess())
                access->disconnect(this);
        });

    connect(d.data(), &SystemContextAccessor::systemContextChanged, this,
        [this, updateAvailablity]()
        {
            const auto context = d->mobileSystemContext();
            if (!context)
                return;

            if (const auto featureAccess = context->featureAccess())
            {
                connect(featureAccess,
                    &core::FeatureAccessWatcher::canUseShareBookmarkChanged,
                    this,
                    updateAvailablity);
            }

            updateAvailablity();
        });
}

ShareBookmarkBackend::~ShareBookmarkBackend()
{
}

timeline::AbstractObjectData* ShareBookmarkBackend::objectData() const
{
    return d->objectData;
}

void ShareBookmarkBackend::setObjectData(timeline::AbstractObjectData* objectData)
{
    if (d->objectData == objectData)
        return;

    d->objectDataConnection.reset();

    d->objectData = objectData;
    resetBookmarkData();
    emit objectDataChanged();

    if (!d->objectData)
        return;

    d->objectDataConnection.reset(connect(
        objectData, &timeline::AbstractObjectData::changed,
        this, &ShareBookmarkBackend::bookmarkChanged));
}

bool ShareBookmarkBackend::isAvailable()
{
    return d->isAvailable;
}

bool ShareBookmarkBackend::isShared() const
{
    if (!d->objectData || !NX_ASSERT(d->systemContext()))
        return false;

    constexpr api::BookmarkShareFilters kSharedBookmarkFilters =
        api::BookmarkShareFilter::shared | api::BookmarkShareFilter::accessible;

    const auto bookmark = d->objectData->convertToBookmark();

    return bookmark.shareable()
        && bookmark.bookmarkMatchesFilter(kSharedBookmarkFilters)
        && d->systemContext()->featureAccess()->canUseShareBookmark();
}

QString ShareBookmarkBackend::bookmarkName() const
{
    return d->bookmark.name;
}

void ShareBookmarkBackend::setBookmarkName(const QString& value)
{
    if (value == d->bookmark.name)
        return;

    d->bookmark.name = value;
    emit bookmarkChanged();
}

QString ShareBookmarkBackend::bookmarkDescription() const
{
    return d->bookmark.description;
}

void ShareBookmarkBackend::setBookmarkDescription(const QString& value)
{
    if (value == d->bookmark.description)
        return;

    d->bookmark.description = value;
    emit bookmarkChanged();
}

qint64 ShareBookmarkBackend::expirationTimeMs() const
{
    return d->bookmark.share.expirationTimeMs.count();
}

QString ShareBookmarkBackend::bookmarkDigest() const
{
    return d->bookmark.share.digest
        ? *d->bookmark.share.digest
        : QString{};
}

bool ShareBookmarkBackend::isExpired() const
{
    return d->bookmark.shareable()
        && d->bookmark.bookmarkMatchesFilter(api::BookmarkShareFilter::shared
            | api::BookmarkShareFilter::expired
            | api::BookmarkShareFilter::hasExpiration);
}

bool ShareBookmarkBackend::isNeverExpires() const
{
    return !d->bookmark.shareable() || !d->bookmark.bookmarkMatchesFilter(
        api::BookmarkShareFilter::shared | api::BookmarkShareFilter::hasExpiration);
}

QString ShareBookmarkBackend::expiresInText() const
{
    if (!NX_ASSERT(d->bookmark.share.expirationTimeMs > qnSyncTime->value()))
        return {};

    const auto timeString =
        [this]()
        {
            const auto duration = d->bookmark.share.expirationTimeMs - qnSyncTime->value();
            if (duration < hours(1))
                return text::HumanReadable::timeSpan(duration, text::HumanReadable::Minutes);
            if (duration < days(1))
                return text::HumanReadable::timeSpan(duration, text::HumanReadable::Hours);
            if (duration < months(1))
                return text::HumanReadable::timeSpan(duration, text::HumanReadable::Days);
            return QString{};
        }();

    return timeString.isEmpty()
        ? QString{}
        : tr("Expires in %1", "%1 is time text like '48 minutes'").arg(timeString);
}

bool ShareBookmarkBackend::share(qint64 expirationTime,
    const QString& password,
    bool showNativeShareSheet)
{
    milliseconds expirationTimeMs = {};
    switch (expirationTime)
    {
        case static_cast<qint64>(ExpiresIn::Hour):
            expirationTimeMs = qnSyncTime->value() + hours(1);
            break;
        case static_cast<qint64>(ExpiresIn::Day):
            expirationTimeMs = qnSyncTime->value() + days(1);
            break;
        case static_cast<qint64>(ExpiresIn::Month):
            expirationTimeMs = qnSyncTime->value() + months(1);
            break;
        case static_cast<qint64>(ExpiresIn::Never):
            expirationTimeMs = 0ms;
            break;
        default:
            expirationTimeMs= milliseconds(expirationTime);
            break;
    }

    const auto context = d->systemContext();
    if (!NX_ASSERT(context))
        return false;

    const auto shareParams = common::BookmarkShareableParams{
        .shareable = true,
        .expirationTimeMs = expirationTimeMs,
        .digest = password == d->bookmark.share.digest
            ? std::optional<QString>{} //< Changes nothing on patch request.
            : password
    };

    return d->updateShareParams(shareParams, showNativeShareSheet);
}

bool ShareBookmarkBackend::stopSharing()
{
    return NX_ASSERT(d->operation != QnCameraBookmarksManager::BookmarkOperation::create)
        && d->updateShareParams({
            .shareable = false,
            .expirationTimeMs = 0ms,
            .digest = ""});
}

void ShareBookmarkBackend::resetBookmarkData()
{
    d->bookmark = {};

    d->setRawResource(d->objectData ? d->objectData->resource().get() : nullptr);

    if (!d->objectData)
        return;

    const auto context = d->systemContext();
    if (!NX_ASSERT(context))
        return;

    d->bookmark = d->objectData->convertToBookmark();
    d->operation = d->bookmark.guid.isNull()
        ? QnCameraBookmarksManager::BookmarkOperation::create
        : QnCameraBookmarksManager::BookmarkOperation::update;

    if (d->bookmark.guid.isNull())
        d->bookmark.guid = nx::Uuid::createUuid();

    emit bookmarkChanged();
}

} // namespace nx::vms::client::mobile
