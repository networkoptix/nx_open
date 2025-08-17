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
#include <nx/network/cloud/cloud_connect_controller.h>
#include <nx/network/http/http_types.h>
#include <nx/network/socket_global.h>
#include <nx/network/url/url_builder.h>
#include <nx/utils/math/math.h>
#include <nx/vms/api/data/bookmark_models.h>
#include <nx/vms/client/core/analytics/analytics_attribute_helper.h>
#include <nx/vms/client/core/client_core_globals.h>
#include <nx/vms/client/core/watchers/feature_access_watcher.h>
#include <nx/vms/client/core/watchers/user_watcher.h>
#include <nx/vms/client/mobile/application_context.h>
#include <nx/vms/client/mobile/system_context.h>
#include <nx/vms/client/mobile/system_context_accessor.h>
#include <nx/vms/client/mobile/ui/share_link_helper.h>
#include <nx/vms/client/mobile/ui/ui_controller.h>
#include <nx/vms/client/mobile/window_context.h>
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
    QPersistentModelIndex modelIndex;

    bool isAvailable = false;
    QnCameraBookmarksManager::BookmarkOperation operation;
    common::CameraBookmark bookmark;

    Private(ShareBookmarkBackend* q);

    void shareBookmarkLink(const QString& bookmarkId) const;
    bool updateShareParams(const common::BookmarkShareableParams& shareParams,
        bool showNativeShareSheet = false);
    void showErrorMessage() const;
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

            executeLater([this]() { showErrorMessage(); }, q);

            return false;
        };

    bookmark.share = shareParams;

    auto callback = nx::utils::guarded(q,
        [this, handleResult, showNativeShareSheet](bool result, const api::BookmarkV3& bookmarkV3)
        {
            if (!handleResult(result))
                return;

            // As we created or updated bookmark - there is no need to create it next time.
            operation = QnCameraBookmarksManager::BookmarkOperation::update;

            bookmark = common::bookmarkFromApi(bookmarkV3);

            if (showNativeShareSheet && bookmark.shareable() && NX_ASSERT(!bookmarkV3.id.isEmpty()))
                shareBookmarkLink(bookmarkV3.id);

            emit q->bookmarkChanged();
        });

    return handleResult(manager->changeBookmarkRest(operation, bookmark, std::move(callback)));
}

void ShareBookmarkBackend::Private::showErrorMessage() const
{
    if (const auto context = mobileSystemContext())
    {
        if (const auto windowContext = context->windowContext())
        {
            windowContext->uiController()->showMessage(
                ShareBookmarkBackend::tr("Error"),
                ShareBookmarkBackend::tr("Cannot share bookmark."));
        }
    }
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

QModelIndex ShareBookmarkBackend::modelIndex() const
{
    return d->modelIndex;
}

void ShareBookmarkBackend::setModelIndex(const QModelIndex& value)
{
    if (d->modelIndex == value)
        return;

    if (d->modelIndex.isValid())
        d->modelIndex.model()->disconnect(this);

    d->modelIndex = value;

    if (d->modelIndex.isValid())
    {
        connect(d->modelIndex.model(), &QAbstractItemModel::dataChanged, this,
            [this](const auto& topLeft, const auto& bottomRight, const auto& /*roles*/)
            {
                const int column = d->modelIndex.column();
                const int row = d->modelIndex.row();

                if (qBetween(topLeft.column(), column, bottomRight.column() + 1)
                    && qBetween(topLeft.row(), row, bottomRight.row() + 1))
                {
                    emit bookmarkChanged();
                }
            });
    }
    resetBookmarkData();

    emit modelIndexChanged();
}

bool ShareBookmarkBackend::isAvailable()
{
    return d->isAvailable;
}

bool ShareBookmarkBackend::isShared() const
{
    return d->modelIndex.data(core::IsSharedBookmark).toBool();
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

    d->updateFromModelIndex(d->modelIndex);

    if (!d->modelIndex.isValid())
        return;

    const auto context = d->systemContext();
    if (!NX_ASSERT(context))
        return;

    if (d->modelIndex.data(core::CameraBookmarkRole).isValid())
    {
        // Update bookmark from bookmarks model index.
        d->operation = QnCameraBookmarksManager::BookmarkOperation::update;
        d->bookmark = d->modelIndex.data(core::CameraBookmarkRole).value<common::CameraBookmark>();
        emit bookmarkChanged();
        return;
    }

    // Construct bookmark from analytics object model index.

    d->operation = QnCameraBookmarksManager::BookmarkOperation::create;

    const auto camera = d->modelIndex.data(core::ResourceRole)
        .value<QnResourcePtr>().dynamicCast<QnMediaResource>();
    const auto timestampMs = d->modelIndex.data(core::TimestampMsRole).value<qint64>();
    d->bookmark.guid = nx::Uuid::createUuid();
    d->bookmark.tags = { detail::BookmarkConstants::objectBasedTagName() };
    d->bookmark.cameraId = camera->getId();
    d->bookmark.creatorId = context->userWatcher()->user()->getId();
    d->bookmark.name = d->modelIndex.data(Qt::DisplayRole).toString();
    d->bookmark.creationTimeStampMs = qnSyncTime->value();
    d->bookmark.startTimeMs = milliseconds(timestampMs);
    static const qint64 kMimDurationMs = 8000;
    d->bookmark.durationMs = milliseconds(
        std::max<qint64>(d->modelIndex.data(core::DurationMsRole).value<qint64>(), kMimDurationMs));
    d->bookmark.description =
        [this, camera, timestampMs]()
        {
            static const QString kTemplate("%1: %2");
            const auto attributes = d->modelIndex.data(core::AnalyticsAttributesRole)
                .value<core::analytics::AttributeList>();

            QStringList result;

            const auto dateTime = QDateTime::fromMSecsSinceEpoch(
                timestampMs, core::ServerTimeWatcher::timeZone(camera));
            result.append(dateTime.toString());
            result.append(kTemplate.arg(tr("Camera"), camera->getName()));

            for (const auto& attribute: attributes)
            {
                result.append(kTemplate.arg(
                    attribute.displayedName, attribute.displayedValues.join(", ")));
            }
            return result.join("\n");

        }();

    emit bookmarkChanged();
}

} // namespace nx::vms::client::mobile
