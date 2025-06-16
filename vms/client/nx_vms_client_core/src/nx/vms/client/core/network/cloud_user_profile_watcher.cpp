// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "cloud_user_profile_watcher.h"

#include <QtCore/QBuffer>
#include <QtGui/QImage>
#include <QtGui/QPainter>

#include <nx/cloud/db/api/account_data.h>
#include <nx/cloud/db/api/account_manager.h>
#include <nx/cloud/db/client/cdb_connection.h>
#include <nx/cloud/db/client/cdb_request_path.h>
#include <nx/utils/async_handler_executor.h>
#include <nx/utils/coro/task_utils.h>
#include <nx/utils/coro/when_all.h>
#include <nx/utils/scoped_connections.h>
#include <nx/vms/client/core/application_context.h>
#include <nx/vms/client/core/network/cloud_api.h>
#include <nx/vms/client/core/network/cloud_status_watcher.h>
#include <nx/vms/client/core/settings/client_core_settings.h>
#include <nx/vms/client/core/skin/color_theme.h>

namespace nx::vms::client::core {

namespace {

using namespace std::chrono;

static constexpr auto kErrorRetryDelay = 5s;
static constexpr auto kUpdateDelay = 20s;

// Converts QImage to data URL, encoding as PNG in base64.
QUrl imageToUrl(const QImage& image)
{
    QByteArray byteArray;
    QBuffer buffer(&byteArray);
    buffer.open(QIODevice::WriteOnly);
    image.save(&buffer, "png");
    const QByteArray base64 = byteArray.toBase64();
    return QString("data:image/png;base64,") + base64.constData();
}

// Returns capitalized initials for full name: "john doe" -> "JD".
QString getInitials(const QString& fullName)
{
    const auto parts = QStringView(fullName).split(' ', Qt::SkipEmptyParts);
    if (parts.empty())
        return {};

    QString initials = parts.first().at(0).toUpper();

    if (parts.size() > 1)
        initials += parts.last().at(0).toUpper();

    return initials;
}

} // namespace

CloudUserProfileWatcher::CloudUserProfileWatcher(QObject* parent):
    base_type(parent)
{
    run();
}

CloudUserProfileWatcher::~CloudUserProfileWatcher()
{
}

QString CloudUserProfileWatcher::fullName() const
{
    return m_statusWatcher->status() == CloudStatusWatcher::Offline
        ? appContext()->coreSettings()->lastCloudFullName()
        : m_fullName;
}

bool CloudUserProfileWatcher::accountBelongsToOrganization() const
{
    return m_accountBelongsToOrganization;
}

QUrl CloudUserProfileWatcher::avatarUrl() const
{
    // This method should return an url for a user avatar from the cloud, but it is not
    // implemented yet on the cloud side.
    // So we generate a local avatar image with initials based on the user's full name.

    static constexpr auto kAvatarSize = 128;
    static constexpr auto kAvatarInitialsFontPixelSize = 48;

    if (fullName().isEmpty())
        return {};

    QImage image(kAvatarSize, kAvatarSize, QImage::Format_RGBA8888_Premultiplied);
    image.fill(Qt::transparent);

    QPainter painter(&image);

    painter.setRenderHint(QPainter::Antialiasing);
    painter.setBrush(nx::vms::client::core::colorTheme()->color("brand_core"));
    painter.setPen(Qt::NoPen);
    painter.drawEllipse(0, 0, kAvatarSize, kAvatarSize);

    QString initials = getInitials(fullName());

    painter.setPen(Qt::black);
    auto font = painter.font();
    font.setPixelSize(kAvatarInitialsFontPixelSize);
    painter.setFont(font);
    painter.drawText(0, 0, kAvatarSize, kAvatarSize, Qt::AlignCenter, initials);

    return imageToUrl(image);
}

using namespace std::chrono_literals;

nx::coro::FireAndForget CloudUserProfileWatcher::run()
{
    co_await nx::coro::guard(this);
    co_await nx::coro::whenProperty(
        this,
        "statusWatcher",
        [](auto value) { return !value.isNull(); });

    for (;;)
    {
        co_await nx::coro::whenProperty(
            m_statusWatcher,
            "status",
            [](auto value) { return value == CloudStatusWatcher::Online; });

        auto [accountData, channelPartnerList] = co_await whenAll(
            cloudGet<nx::cloud::db::api::AccountData>(
                m_statusWatcher,
                nx::cloud::db::kAccountSelfPath),
            cloudGet<ChannelPartnerList>(
                m_statusWatcher,
                "/partners/api/v2/channel_partners/"));

        if (!accountData)
        {
            NX_WARNING(this,
                "Error getting account data: %1",
                cloud::db::api::toString(accountData.error()));
            co_await nx::coro::qtTimer(kErrorRetryDelay);
            continue;
        }

        if (m_fullName != accountData->fullName)
        {
            m_fullName = QString::fromStdString(accountData->fullName);
            appContext()->coreSettings()->lastCloudFullName = m_fullName;
            emit fullNameChanged();
            emit avatarUrlChanged();
        }

        if (accountData->accountBelongsToOrganization
            && m_accountBelongsToOrganization != *accountData->accountBelongsToOrganization)
        {
            m_accountBelongsToOrganization = *accountData->accountBelongsToOrganization;
            emit accountBelongsToOrganizationChanged();
        }

        co_await nx::coro::qtTimer(kUpdateDelay);
    }
}

} // nx::vms::client::core
