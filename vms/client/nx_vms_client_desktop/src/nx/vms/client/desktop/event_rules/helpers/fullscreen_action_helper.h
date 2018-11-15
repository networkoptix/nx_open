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
    static QSet<QnUuid> cameraIds(const QnBusinessRuleViewModel* model);
    static QSet<QnUuid> setCamera(const QnBusinessRuleViewModel* model,
        const QnVirtualCameraResourcePtr& camera);
    static QSet<QnUuid> setCameraIds(const QnBusinessRuleViewModel* model,
        const QSet<QnUuid>& cameraIds);

    static QnLayoutResourceList layouts(const QnBusinessRuleViewModel* model);
    static QSet<QnUuid> layoutIds(const QnBusinessRuleViewModel* model);
    static QSet<QnUuid> setLayouts(const QnBusinessRuleViewModel* model,
        const QnLayoutResourceList& layouts);
    static QSet<QnUuid> setLayoutIds(const QnBusinessRuleViewModel* model,
        const QSet<QnUuid>& layoutIds);

    static bool cameraExistOnLayouts(const QnBusinessRuleViewModel* model);
};

} // namespace nx::vms::client::desktop
