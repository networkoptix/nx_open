#include "business_action_parameters.h"

namespace QnBusinessActionParameters {

    static QLatin1String soundUrl("soundUrl");

    QString getSoundUrl(const QnBusinessParams &params) {
        return params.value(soundUrl, QString()).toString();
    }

    void setSoundUrl(QnBusinessParams* params, const QString &value) {
        (*params)[soundUrl] = value;
    }


    static QLatin1String emailAddress("emailAddress");

    QString getEmailAddress(const QnBusinessParams &params) {
        return params.value(emailAddress, QString()).toString();
    }

    void setEmailAddress(QnBusinessParams* params, const QString &value) {
        (*params)[emailAddress] = value;
    }

    static QLatin1String userGroup("userGroup");

    UserGroup getUserGroup(const QnBusinessParams &params) {
        return (UserGroup)params.value(userGroup, int(EveryOne)).toInt();
    }

    void setUserGroup(QnBusinessParams* params, const UserGroup value) {
        (*params)[userGroup] = (int)value;
    }


}
