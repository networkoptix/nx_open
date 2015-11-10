#ifndef BOOKMARK_BUSINESS_ACTION_H
#define BOOKMARK_BUSINESS_ACTION_H

#include <business/actions/abstract_business_action.h>

#include <core/resource/camera_bookmark_fwd.h>

class QnBookmarkBusinessAction: public QnAbstractBusinessAction {
    typedef QnAbstractBusinessAction base_type;
public:
    explicit QnBookmarkBusinessAction(const QnBusinessEventParameters &runtimeParams);
    ~QnBookmarkBusinessAction() {}

    QString getName() const;
    QString getDescription() const;
    qint64 getTimeout() const;
    QnCameraBookmarkTags getTags() const;
};

typedef QSharedPointer<QnBookmarkBusinessAction> QnBookmarkBusinessActionPtr;

#endif  //BOOKMARK_BUSINESS_ACTION_H
