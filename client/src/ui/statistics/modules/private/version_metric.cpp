
#include "version_metric.h"

#include <utils/common/app_info.h>

VersionMetric::VersionMetric()
    : base_type()
{}

VersionMetric::~VersionMetric()
{}

bool VersionMetric::isSignificant() const
{
    return true;
}

QString VersionMetric::value() const
{
    return QnAppInfo::applicationFullVersion();
}

void VersionMetric::reset()
{}