// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QCoreApplication>

#include <core/resource/resource_fwd.h>
#include <nx/utils/uuid.h>

class QnBusinessRuleViewModel;

namespace nx::vms::client::desktop {

class FullscreenActionHelper
{
    Q_DECLARE_TR_FUNCTIONS(FullscreenActionHelper)
public:
    static bool isCameraValid(const QnBusinessRuleViewModel* model);
    static QString cameraText(const QnBusinessRuleViewModel* model);
    static QIcon cameraIcon(const QnBusinessRuleViewModel* model);

    static bool isLayoutValid(const QnBusinessRuleViewModel* model);
    static QString layoutText(const QnBusinessRuleViewModel* model);
    static QIcon layoutIcon(const QnBusinessRuleViewModel* model);

    static QString tableCellText(const QnBusinessRuleViewModel* model);
    static QIcon tableCellIcon(const QnBusinessRuleViewModel* model);

    static QnVirtualCameraResourcePtr camera(const QnBusinessRuleViewModel* model);
    static QSet<nx::Uuid> cameraIds(const QnBusinessRuleViewModel* model);
    static QSet<nx::Uuid> setCamera(const QnBusinessRuleViewModel* model,
        const QnVirtualCameraResourcePtr& camera);
    static QSet<nx::Uuid> setCameraIds(const QnBusinessRuleViewModel* model,
        const QSet<nx::Uuid>& cameraIds);

    static QnLayoutResourceList layouts(const QnBusinessRuleViewModel* model);
    static QSet<nx::Uuid> layoutIds(const QnBusinessRuleViewModel* model);
    static QSet<nx::Uuid> setLayouts(const QnBusinessRuleViewModel* model,
        const QnLayoutResourceList& layouts);
    static QSet<nx::Uuid> setLayoutIds(const QnBusinessRuleViewModel* model,
        const QSet<nx::Uuid>& layoutIds);

    static bool cameraExistOnLayouts(const QnBusinessRuleViewModel* model);
};

} // namespace nx::vms::client::desktop
