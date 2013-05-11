#ifndef BUSINESS_ACTION_PARAMETERS_H
#define BUSINESS_ACTION_PARAMETERS_H

#include <business/business_logic_common.h>

//TODO: #GDM move QnStreamQuality OUT OF THERE!
#include <core/resource/media_resource.h>

namespace QnBusinessActionParameters {

    // Play Sound

    QString getSoundUrl(const QnBusinessParams &params);
    void setSoundUrl(QnBusinessParams* params, const QString &value);

    // Email

    QString getEmailAddress(const QnBusinessParams &params);
    void setEmailAddress(QnBusinessParams* params, const QString &value);

    // Popups and System Health

    enum UserGroup {
        EveryOne,
        AdminOnly
    };

    UserGroup getUserGroup(const QnBusinessParams &params);
    void setUserGroup(QnBusinessParams* params, const UserGroup value);

    // Recording

    int getFps(const QnBusinessParams &params);
    void setFps(QnBusinessParams* params, int value);

    QnStreamQuality getStreamQuality(const QnBusinessParams &params);
    void setStreamQuality(QnBusinessParams* params, QnStreamQuality value);

    int getRecordDuration(const QnBusinessParams &params);
    void setRecordDuration(QnBusinessParams* params, int value);

    int getRecordBefore(const QnBusinessParams &params);
    void setRecordBefore(QnBusinessParams* params, int value);

    int getRecordAfter(const QnBusinessParams &params);
    void setRecordAfter(QnBusinessParams* params, int value);

    // Camera Output

    QString getRelayOutputId(const QnBusinessParams &params);
    void setRelayOutputId(QnBusinessParams* params, const QString &value);

    int getRelayAutoResetTimeout(const QnBusinessParams &params);
    void setRelayAutoResetTimeout(QnBusinessParams* params, int value);

}

#endif // BUSINESS_ACTION_PARAMETERS_H
