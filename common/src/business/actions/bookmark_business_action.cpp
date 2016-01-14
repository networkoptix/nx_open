
#include "bookmark_business_action.h"

QnBookmarkBusinessAction::QnBookmarkBusinessAction(const QnBusinessEventParameters &runtimeParams)
    : QnCommonBusinessAction(QnBusiness::BookmarkAction, runtimeParams)
{}

QString QnBookmarkBusinessAction::getExternalUniqKey() const
{
    return lit("%1%2")
        .arg(QnCommonBusinessAction::getExternalUniqKey())
        .arg(getBusinessRuleId().toString());
}
