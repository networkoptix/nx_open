#ifndef BUSINESS_ACTION_PARAMETERS_H
#define BUSINESS_ACTION_PARAMETERS_H

#include <business/business_logic_common.h>

//TODO: #GDM move QnStreamQuality OUT OF THERE!
#include <core/resource/media_resource.h>

class QnBusinessActionParameters 
{
public:

    enum Params 
    {
        soundUrlParam,
        emailAddressParam,
        userGroupParam,
        fpsParam,
        qualityParam,
        durationParam,
        beforeParam,
        afterParam,
        relayOutputIDParam,
        relayAutoResetTimeoutParam,
        inputPortIdParam,
        keyParam,

        CountParam
    };

    QnBusinessActionParameters();

    // Play Sound

    QString getSoundUrl() const;
    void setSoundUrl(const QString &value);

    // Email

    QString getEmailAddress() const;
    void setEmailAddress(const QString &value);

    // Popups and System Health

    enum UserGroup {
        EveryOne,
        AdminOnly
    };

    UserGroup getUserGroup() const;
    void setUserGroup(const UserGroup value);

    // Recording

    int getFps() const;
    void setFps(int value);

    QnStreamQuality getStreamQuality() const;
    void setStreamQuality(QnStreamQuality value);

    int getRecordDuration() const;
    void setRecordDuration(int value);

    int getRecordBefore() const;
    void setRecordBefore(int value);

    int getRecordAfter() const;
    void setRecordAfter(int value);

    // Camera Output

    QString getRelayOutputId() const;
    void setRelayOutputId(const QString &value);

    int getRelayAutoResetTimeout() const;
    void setRelayAutoResetTimeout(int value);

    QString getParamsKey() const;
    void setParamsKey(QString value);

    QString getInputPortId() const;
    void setInputPortId(const QString &value);

    // convert/serialize/deserialize functions

    static QnBusinessActionParameters deserialize(const QByteArray& value);
    QByteArray serialize() const;

    QnBusinessParams toBusinessParams() const;
    static QnBusinessActionParameters fromBusinessParams(const QnBusinessParams& bParams);

    QVariant& operator[](int index);
    const QVariant& operator[](int index) const;

    /*
    * Returns true if all parameters have default values
    */
    bool isDefault() const;
private:
    static int getParamIndex(const QString& key);
private:
    QVector<QVariant> m_params;
};

#endif // BUSINESS_ACTION_PARAMETERS_H
