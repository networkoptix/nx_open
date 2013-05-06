#include "business_action_parameters.h"

#include "utils/common/enum_name_mapper.h"

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


    static QLatin1String fps("fps");
    static QLatin1String quality("quality");
    static QLatin1String duration("duration");
    static QLatin1String before("before");
    static QLatin1String after("after");

    int getFps(const QnBusinessParams &params) {
        return params.value(fps, 10).toInt();
    }

    void setFps(QnBusinessParams* params, int value) {
        (*params)[fps] = value;
    }

    QnStreamQuality getStreamQuality(const QnBusinessParams &params) {
        return (QnStreamQuality)params.value(quality, (int)QnQualityHighest).toInt();
    }

    void setStreamQuality(QnBusinessParams* params, QnStreamQuality value) {
        (*params)[quality] = (int)value;
    }

    int getRecordDuration(const QnBusinessParams &params) {
        return params.value(duration, 0).toInt();
    }

    void setRecordDuration(QnBusinessParams* params, int value) {
        (*params)[duration] = value;
    }

    int getRecordBefore(const QnBusinessParams &params) {
        return params.value(before, 0).toInt();
    }

    void setRecordBefore(QnBusinessParams* params, int value) {
        (*params)[before] = value;
    }

    int getRecordAfter(const QnBusinessParams &params) {
        return params.value(after, 0).toInt();
    }

    void setRecordAfter(QnBusinessParams* params, int value)  {
        (*params)[after] = value;
    }

    static QLatin1String relayOutputId("relayOutputID");
    static QLatin1String relayAutoResetTimeout("relayAutoResetTimeout");

    QString getRelayOutputId(const QnBusinessParams &params) {
        return params.value(relayOutputId, QString()).toString();
    }

    void setRelayOutputId(QnBusinessParams* params, const QString &value) {
        (*params)[relayOutputId] = value;
    }

    int getRelayAutoResetTimeout(const QnBusinessParams &params) {
        return params.value(relayAutoResetTimeout, 0).toInt();
    }

    void setRelayAutoResetTimeout(QnBusinessParams* params, int value) {
        (*params)[relayAutoResetTimeout] = value;
    }

}
