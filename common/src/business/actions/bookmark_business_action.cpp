
#include "bookmark_business_action.h"

QnBookmarkBusinessAction::QnBookmarkBusinessAction(const QnBusinessEventParameters &runtimeParams)
    : base_type(QnBusiness::BookmarkAction, runtimeParams)
{}

QString QnBookmarkBusinessAction::getExternalUniqKey() const
{
    return lit("%1%2")
        .arg(base_type::getExternalUniqKey())
        .arg(getBusinessRuleId().toString());
}
