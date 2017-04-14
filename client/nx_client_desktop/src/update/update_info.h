#pragma once

#include <utils/common/software_version.h>
#include <nx/fusion/model_functions_fwd.h>

struct QnUpdateInfo
{
    QnUpdateInfo();

    QnSoftwareVersion currentRelease;   /**< Published release version. */
    QUrl releaseNotesUrl;               /**< Link to release notes. */
    qint64 releaseDateMs;               /**< Release date - in msecs since epoch. */
    int releaseDeliveryDays;            /**< Maximum days for release delivery. */
};

QN_FUSION_DECLARE_FUNCTIONS(QnUpdateInfo, (datastream)(metatype))
