#pragma once

#include <QtCore/QCoreApplication>

#include <core/resource/resource_fwd.h>

#include <nx/utils/uuid.h>

class QnBusinessRuleViewModel;

namespace nx::vms::client::desktop {

class ExitFullscreenActionHelper
{
    Q_DECLARE_TR_FUNCTIONS(ExitFullscreenActionHelper)
public:
    static bool isLayoutValid(const QnBusinessRuleViewModel* model);
    static QString layoutText(const QnBusinessRuleViewModel* model);
    static QIcon layoutIcon(const QnBusinessRuleViewModel* model);

    static QString tableCellText(const QnBusinessRuleViewModel* model);
    static QIcon tableCellIcon(const QnBusinessRuleViewModel* model);

    static QnLayoutResourceList layouts(const QnBusinessRuleViewModel* model);
    static QSet<QnUuid> layoutIds(const QnBusinessRuleViewModel* model);
    static QSet<QnUuid> setLayouts(const QnBusinessRuleViewModel* model,
        const QnLayoutResourceList& layouts);
    static QSet<QnUuid> setLayoutIds(const QnBusinessRuleViewModel* model,
        const QSet<QnUuid>& layoutIds);
};

} // namespace nx::vms::client::desktop
