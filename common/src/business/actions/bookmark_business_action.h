
#pragma once

#include <business/actions/common_business_action.h>

class QnBookmarkBusinessAction : public QnCommonBusinessAction
{
    typedef QnCommonBusinessAction base_type;
public:
    QnBookmarkBusinessAction(const QnBusinessEventParameters &runtimeParams);

    virtual QString getExternalUniqKey() const override;

};