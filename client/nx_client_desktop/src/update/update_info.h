#pragma once

#include <utils/common/software_version.h>
#include <nx/fusion/model_functions_fwd.h>
#include <nx/utils/url.h>

struct QnUpdateInfo
{
    QnSoftwareVersion currentRelease;   //< Published release version.
    nx::utils::Url releaseNotesUrl;     //< Link to release notes.
    qint64 releaseDateMs = 0;           //< Release date - in msecs since epoch.
    int releaseDeliveryDays = 0;        //< Maximum days for release delivery.
    QString description;                //< Update description that may be displayed to the user.
};
#define QnUpdateInfo_Fields (currentRelease)(releaseNotesUrl)(releaseDateMs)(releaseDeliveryDays)\
    (description)

QN_FUSION_DECLARE_FUNCTIONS(QnUpdateInfo, (datastream)(metatype))
