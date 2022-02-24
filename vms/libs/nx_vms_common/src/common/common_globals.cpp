// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "common_globals.h"

namespace Qn {

nx::vms::api::StreamIndex toStreamIndex(ConnectionRole role)
{
    using namespace nx::vms::api;
    switch (role)
    {
        case ConnectionRole::CR_LiveVideo:
            return StreamIndex::primary;
        case ConnectionRole::CR_SecondaryLiveVideo:
            return StreamIndex::secondary;
        default:
            return StreamIndex::undefined;
    }
}

Qn::ConnectionRole fromStreamIndex(nx::vms::api::StreamIndex streamIndex)
{
    using namespace nx::vms::api;
    switch (streamIndex)
    {
        case StreamIndex::primary:
            return ConnectionRole::CR_LiveVideo;
        case StreamIndex::secondary:
            return ConnectionRole::CR_SecondaryLiveVideo;
        default:
            return ConnectionRole::CR_Default;
    }
}

QString toString(UserRole value)
{
    switch (value)
    {
        case UserRole::customUserRole:
            return "customUserRole";
        case UserRole::customPermissions:
            return "customPermissions";
        case UserRole::owner:
            return "owner";
        case UserRole::administrator:
            return "administrator";
        case UserRole::advancedViewer:
            return "advancedViewer";
        case UserRole::viewer:
            return "viewer";
        case UserRole::liveViewer:
            return "liveViewer";
        default:
            NX_ASSERT(false, "Unexpected enumeration value %1", (int) value);
            return "unknown";
    }
}

QString toString(Qn::TimePeriodContent value)
{
    switch (value)
    {
        case Qn::TimePeriodContent::RecordingContent:
            return "recorded";
        case Qn::TimePeriodContent::MotionContent:
            return "motion";
        case Qn::TimePeriodContent::AnalyticsContent:
            return "analytics";
        default:
            break;
    }
    NX_ASSERT(false, "Should never get here");
    return QString::number(value);
}

} // namespace Qn
