// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>
#include <optional>

#include <QtCore/QCoreApplication> //for Q_DECLARE_TR_FUNCTIONS
#include <QtGui/QValidator>

#include <common/common_globals.h>
#include <core/resource/resource_fwd.h>
#include <nx/network/socket_common.h>
#include <nx/utils/translatable_string.h>
#include <nx/utils/uuid.h>

namespace nx::vms::common { class SystemContext; }

namespace nx::vms::rules {

struct UuidSelection;

class NX_VMS_RULES_API Strings
{
    Q_DECLARE_TR_FUNCTIONS(Strings)

public:
    static QString timestamp(
        std::chrono::microseconds eventTimestamp,
        int eventCount,
        bool html = false);
    static QString timestampDate(std::chrono::microseconds eventTimestamp);
    static QString timestampTime(std::chrono::microseconds eventTimestamp);
    static QString plugin(common::SystemContext* context, nx::Uuid pluginId);

    static QString resource(
        common::SystemContext* context,
        nx::Uuid resourceId,
        Qn::ResourceInfoLevel detailLevel = Qn::ResourceInfoLevel::RI_NameOnly);

    static QString resource(
        const QnResourcePtr& resource,
        Qn::ResourceInfoLevel detailLevel = Qn::ResourceInfoLevel::RI_NameOnly);

    static QString resourceIp(const QnResourcePtr& resource);

    enum Url { localIp, publicIp, cloud };

    /**
     * Construct url, opening webadmin view for the given camera at the given time.
     * @param context System Context.
     * @param id Camera id.
     * @param timestamp Timestamp.
     * @param urlType Url type. Actual for server-side only.
     * @param proxyAddress Force proxy address to be used. Actual for the client side only.
     */
    static QString urlForCamera(
        const QnVirtualCameraResourcePtr& camera,
        std::chrono::microseconds timestamp,
        Url urlType,
        const std::optional<nx::network::SocketAddress>& proxyAddress = std::nullopt);

    static QString subjects(common::SystemContext* context, const UuidSelection& selection);

    static QString eventName(common::SystemContext* context, const QString& type);
    // Keep in sync with StringsHelper::eventDetails(). Includes reason.
    static QStringList eventDetails(const QVariantMap& details);

    static QString actionName(common::SystemContext* context, const QString& type);

    static QString anyEvent();
    static QString analyticsEvents();
    static QString anyAnalyticsEvent();
    static QString cameraIssues_internal(); /**< Device-independent fixed string. */
    static QString deviceIssues_internal(); /**< Device-independent fixed string. */
    static QString deviceIssues(common::SystemContext* context);
    static QString anyCameraIssue_internal(); /**< Device-independent fixed string. */
    static QString anyDeviceIssue_internal(); /**< Device-independent fixed string. */
    static QString anyDeviceIssue(common::SystemContext* context);
    static QString serverEvents();
    static QString anyServerEvent();

    static QString invalidFieldType();
    static QString unexpectedPolicy();
    static QString selectServer();
    static QString selectUser();
    static QString selectCamera(common::SystemContext* context, bool allowMultipleSelection = true);
    static QString noSuitableServers(QValidator::State state);

    static TranslatableString at();
    static TranslatableString to();
    static TranslatableString occursAt();
    static TranslatableString beginWhen();
    static TranslatableString duration();
    static TranslatableString preRecording();
    static TranslatableString postRecording();
    static TranslatableString intervalOfAction();
    static TranslatableString fixedDuration();
    static TranslatableString state();
    static TranslatableString volume();
    static TranslatableString rewind();
    static TranslatableString onLayout();
    static TranslatableString eventDevices();
    static TranslatableString ofType();
    static TranslatableString andCaption();
    static TranslatableString andDescription();
};

} // namespace nx::vms::rules
