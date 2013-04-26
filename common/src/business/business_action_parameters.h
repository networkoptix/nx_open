#ifndef BUSINESS_ACTION_PARAMETERS_H
#define BUSINESS_ACTION_PARAMETERS_H

#include <business/business_logic_common.h>

namespace QnBusinessActionParameters {

    QString getSoundUrl(const QnBusinessParams &params);
    void setSoundUrl(QnBusinessParams* params, const QString &value);

    QString getEmailAddress(const QnBusinessParams &params);
    void setEmailAddress(QnBusinessParams* params, const QString &value);

    enum UserGroup {
        EveryOne,
        AdminOnly
    };

    /** UserGroup contains 1 if action is Admin-Only, otherwise 0. */
    UserGroup getUserGroup(const QnBusinessParams &params);
    void setUserGroup(QnBusinessParams* params, const UserGroup value);

}

#endif // BUSINESS_ACTION_PARAMETERS_H
