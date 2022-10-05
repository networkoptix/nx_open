// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <client/client_model_types.h>

#include <QtCore/QFileInfo>

#include <client/client_globals.h>
#include <client/client_meta_types.h>
#include <nx/fusion/model_functions.h>
#include <nx/reflect/compare.h>

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(QnLicenseWarningState, (datastream), (lastWarningTime));
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(QnBackgroundImage, (json), QnBackgroundImage_Fields, (brief, true));

qreal QnBackgroundImage::actualImageOpacity() const
{
    return enabled ? opacity : 0.0;
}

QnBackgroundImage QnBackgroundImage::defaultBackground()
{
    static const QString kDefaultBackgroundPath = ":/skin/background.png";
    if (!QFileInfo::exists(kDefaultBackgroundPath))
        return {};

    QnBackgroundImage result;
    result.enabled = true;
    result.name = kDefaultBackgroundPath;
    result.mode = Qn::ImageBehavior::Crop;
    return result;
}

bool QnBackgroundImage::operator==(const QnBackgroundImage& other) const
{
    return nx::reflect::equals(*this, other);
}

QnWorkbenchState::QnWorkbenchState()
{

}

bool QnWorkbenchState::isValid() const
{
    return !userId.isNull()
        && !localSystemId.isNull();
}

namespace nx::vms::common {
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(ResourceDescriptor,
    (json),
    ResourceDescriptor_Fields,
    (brief, true))
} // namespace nx::vms::common

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(QnLayoutItemData, (json),
    QnWorkbenchStateUnsavedLayoutItem_Fields);
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(QnWorkbenchState::UnsavedLayout, (json),
    QnWorkbenchStateUnsavedLayout_Fields, (brief, true));
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(QnWorkbenchState, (json), QnWorkbenchState_Fields, (brief, true));
