#ifndef POPUP_BUSINESS_ACTION_H
#define POPUP_BUSINESS_ACTION_H

#include "abstract_business_action.h"

namespace BusinessActionParameters {
    quint64 getUserGroup(const QnBusinessParams &params);
    void setUserGroup(QnBusinessParams* params, const quint64 value);
}

class QnPopupBusinessAction : public QnAbstractBusinessAction
{
    typedef QnAbstractBusinessAction base_type;
public:
    QnPopupBusinessAction();
};

#endif // POPUP_BUSINESS_ACTION_H
