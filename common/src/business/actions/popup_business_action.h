#ifndef POPUP_BUSINESS_ACTION_H
#define POPUP_BUSINESS_ACTION_H

#include "abstract_business_action.h"

namespace BusinessActionParameters {
    /** UserGroup contains 1 if action is Admin-Only, otherwise 0. */
    quint64 getUserGroup(const QnBusinessParams &params);
    void setUserGroup(QnBusinessParams* params, const quint64 value);
}

class QnPopupBusinessAction : public QnAbstractBusinessAction
{
    typedef QnAbstractBusinessAction base_type;
public:
    QnPopupBusinessAction(const QnBusinessParams &runtimeParams);
};

typedef QSharedPointer<QnPopupBusinessAction> QnPopupBusinessActionPtr;

#endif // POPUP_BUSINESS_ACTION_H
