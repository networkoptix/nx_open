#include "bookmark_business_action.h"

QnBookmarkBusinessAction::QnBookmarkBusinessAction(const QnBusinessEventParameters &runtimeParams):
    base_type(QnBusiness::BookmarkAction, runtimeParams)
{
}

QString QnBookmarkBusinessAction::getName() const {
    return QString();
}

QString QnBookmarkBusinessAction::getDescription() const {
    return QString();
}

qint64 QnBookmarkBusinessAction::getTimeout() const {
    return -1;
}

QnCameraBookmarkTags QnBookmarkBusinessAction::getTags() const {
    return QnCameraBookmarkTags();
}
