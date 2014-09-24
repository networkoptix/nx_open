#ifndef BUSINESS_ACTION_PARAMETERS_H
#define BUSINESS_ACTION_PARAMETERS_H

#include <business/business_fwd.h>

//TODO: #rvasilenko adding new parameter is TOO complex. Structure can be simplified --gdm

class QnBusinessActionParameters 
{
public:
    enum Param {
        SoundUrlParam,
        EmailAddressParam,
        UserGroupParam,
        FpsParam,
        QualityParam,
        DurationParam,
        BeforeParam,
        AfterParam,
        RelayOutputIdParam,
        EelayAutoResetTimeoutParam,
        InputPortIdParam,
        KeyParam,
        SayTextParam,

        ParamCount
    };

    enum UserGroup {
        EveryOne  = 0,
        AdminOnly = 1
    };

    QnBusinessActionParameters();

    // Play Sound

    QString getSoundUrl() const;
    void setSoundUrl(const QString &value);

    // Email

    QString getEmailAddress() const;
    void setEmailAddress(const QString &value);

    // Popups and System Health

    UserGroup getUserGroup() const;
    void setUserGroup(const UserGroup value);

    // Recording

    int getFps() const;
    void setFps(int value);

    Qn::StreamQuality getStreamQuality() const;
    void setStreamQuality(Qn::StreamQuality value);

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

    // TODO: #GDM #Business this one seems not to be used anywhere. Why? Can it be removed? Ask Roma/Andrey.
    QString getParamsKey() const;
    void setParamsKey(QString value);

    QString getInputPortId() const;
    void setInputPortId(const QString &value);

    // Say Text
    QString getSayText() const;
    void setSayText(const QString &value);

    // convert/serialize/deserialize functions

    static QnBusinessActionParameters deserialize(const QByteArray& value);
    QByteArray serialize() const;

    QnBusinessParams toBusinessParams() const;
    static QnBusinessActionParameters fromBusinessParams(const QnBusinessParams& bParams);

    /** 
     * \returns                        Whether all parameters have default values. 
     */
    bool isDefault() const;
    bool equalTo(const QnBusinessActionParameters& other) const;

private:
    static int getParamIndex(const QString& key);

private:
    QString m_soundUrl;
    QString m_emailAddress;
    int m_userGroup;
    int m_fps;
    int m_streamQuality;
    int m_recordingDuration;
    int m_before;
    int m_after;
    QString m_relayOutputID;
    int m_relayAutoResetTimeout;
    QString m_inputPortId;
    QString m_keyParam;
    QString m_sayText;
};

QByteArray serializeBusinessParams(const QnBusinessParams& value);
QnBusinessParams deserializeBusinessParams(const QByteArray& value);

#endif // BUSINESS_ACTION_PARAMETERS_H
