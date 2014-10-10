#include "business_action_parameters.h"

#include <utils/serialization/json_functions.h>

// TODO: #Elric #enum use QN_DEFINE_EXPLICIT_ENULEXICAL_FUNCTIONS

static QLatin1String PARANAMES[] =
{
    QLatin1String("soundUrl"),
    QLatin1String("emailAddress"),
    QLatin1String("userGroup"),
    QLatin1String("fps"),
    QLatin1String("quality"),
    QLatin1String("duration"),
    QLatin1String("before"),
    QLatin1String("after"),
    QLatin1String("relayOutputID"),
    QLatin1String("relayAutoResetTimeout"),
    QLatin1String("inputPortId"),
    QLatin1String("paramsKey"),
    QLatin1String("sayText"),
};

static const char DELIMITER('$');

QnBusinessActionParameters::QnBusinessActionParameters() {
    userGroup = EveryOne;
    fps = 10;
    streamQuality = Qn::QualityHighest;
    recordingDuration = 0;
    recordAfter = 0;
    relayAutoResetTimeout = 0;
}

inline int toInt(const QByteArray& ba)
{
    const char* curPtr = ba.data();
    const char* end = curPtr + ba.size();
    int result = 0;
    for(; curPtr < end; ++curPtr)
    {
        if (*curPtr < '0' || *curPtr > '9')
            return result;
        result = result * 10 + (*curPtr - '0');
    }
    return result;
}

QnBusinessActionParameters QnBusinessActionParameters::unpack(const QByteArray& value)
{
    QnBusinessActionParameters result;

    if (value.isEmpty())
        return result;

    int i = 0;
    int prevPos = -1;
    while (prevPos < value.size() && i < ParamCount)
    {
        int nextPos = value.indexOf(DELIMITER, prevPos+1);
        if (nextPos == -1)
            nextPos = value.size();

        QByteArray field(value.data() + prevPos + 1, nextPos - prevPos - 1);
        switch ((Param) i)
        {
            case SoundUrlParam:
                result.soundUrl = QString::fromUtf8(field.data(), field.size());
                break;
            case EmailAddressParam:
                result.emailAddress = QString::fromUtf8(field.data(), field.size());
                break;
            case UserGroupParam:
                result.userGroup = static_cast<UserGroup>(toInt(field));
                break;
            case FpsParam:
                result.fps = toInt(field);
                break;
            case QualityParam:
                result.streamQuality = static_cast<Qn::StreamQuality>(toInt(field));
                break;
            case DurationParam:
                result.recordingDuration = toInt(field);
                break;
            case RecordBeforeParam:
                /* Record-before setup is forbidden */
                break;
            case RecordAfterParam:
                result.recordAfter = toInt(field);
                break;
            case RelayOutputIdParam:
                result.relayOutputId = QString::fromUtf8(field.data(), field.size());
                break;
            case RelayAutoResetTimeoutParam:
                result.relayAutoResetTimeout = toInt(field);
                break;
            case InputPortIdParam:
                result.inputPortId = QString::fromUtf8(field.data(), field.size());
                break;
            case KeyParam:
                result.keyParam = QString::fromUtf8(field.data(), field.size());
                break;
            case SayTextParam:
                result.sayText = QString::fromUtf8(field.data(), field.size());
                break;
            default:
                break;
        }
        prevPos = nextPos;
        i++;
    }

    return result;
}

bool QnBusinessActionParameters::isDefault() const
{
    static QnBusinessActionParameters empty;
    return equalTo(empty);
}

bool QnBusinessActionParameters::equalTo(const QnBusinessActionParameters& other) const {
    if (soundUrl != other.soundUrl)
        return false;
    if (emailAddress != other.emailAddress)
        return false;
    if (userGroup != other.userGroup)
        return false;
    if (fps != other.fps)
        return false;
    if (streamQuality != other.streamQuality)
        return false;
    if (recordingDuration != other.recordingDuration)
        return false;
    if (recordAfter != other.recordAfter)
        return false;
    if (relayOutputId != other.relayOutputId)
        return false;
    if (relayAutoResetTimeout != other.relayAutoResetTimeout)
        return false;
    if (inputPortId != other.inputPortId)
        return false;
    if (keyParam != other.keyParam)
        return false;
    if (sayText != other.sayText)
        return false;

    return true;
}

static void serializeIntParam(QByteArray& result, qint64 value, qint64 defaultValue)
{
    if (value != defaultValue)
        result += QByteArray::number(value);
    result += DELIMITER;
}

static void serializeStringParam(QByteArray& result, const QString& value, const QString& defaultValue)
{
    if (value != defaultValue)
        result += value.toUtf8();
    result += DELIMITER;
}

QByteArray QnBusinessActionParameters::pack() const
{
    QByteArray result;

    static QnBusinessActionParameters defaultParams;
    serializeStringParam(result, soundUrl, defaultParams.soundUrl);
    serializeStringParam(result, emailAddress, defaultParams.emailAddress);
    serializeIntParam(result, userGroup, defaultParams.userGroup);
    serializeIntParam(result, fps, defaultParams.fps);
    serializeIntParam(result, streamQuality, defaultParams.streamQuality);
    serializeIntParam(result, recordingDuration, defaultParams.recordingDuration);
    serializeIntParam(result, 0, 0);    /* Record-before setup is forbidden, leaving place to maintain compatibility. */
    serializeIntParam(result, recordAfter, defaultParams.recordAfter);
    serializeStringParam(result, relayOutputId, defaultParams.relayOutputId);
    serializeIntParam(result, relayAutoResetTimeout, defaultParams.relayAutoResetTimeout);
    serializeStringParam(result, inputPortId, defaultParams.inputPortId);
    serializeStringParam(result, keyParam, defaultParams.keyParam);
    serializeStringParam(result, sayText, defaultParams.sayText);

    return result;
}

QnBusinessParams QnBusinessActionParameters::toBusinessParams() const
{
    static QnBusinessActionParameters defaultParams;
    QnBusinessParams result;

    if (soundUrl != defaultParams.soundUrl)
        result.insert(PARANAMES[SoundUrlParam], soundUrl);
    if (emailAddress != defaultParams.emailAddress)
        result.insert(PARANAMES[EmailAddressParam], emailAddress);
    if (userGroup != defaultParams.userGroup)
        result.insert(PARANAMES[UserGroupParam], userGroup);
    if (fps != defaultParams.fps)
        result.insert(PARANAMES[FpsParam], fps);
    if (streamQuality != defaultParams.streamQuality)
        result.insert(PARANAMES[QualityParam], streamQuality);
    if (recordingDuration != defaultParams.recordingDuration)
        result.insert(PARANAMES[DurationParam], recordingDuration);
    if (recordAfter != defaultParams.recordAfter)
        result.insert(PARANAMES[RecordAfterParam], recordAfter);
    if (relayOutputId != defaultParams.relayOutputId)
        result.insert(PARANAMES[RelayOutputIdParam], relayOutputId);
    if (relayAutoResetTimeout != defaultParams.relayAutoResetTimeout)
        result.insert(PARANAMES[RelayAutoResetTimeoutParam], relayAutoResetTimeout);
    if (inputPortId != defaultParams.inputPortId)
        result.insert(PARANAMES[InputPortIdParam], inputPortId);
    if (keyParam != defaultParams.keyParam)
        result.insert(PARANAMES[KeyParam], keyParam);
    if (sayText != defaultParams.sayText)
        result.insert(PARANAMES[SayTextParam], sayText);

    return result;
}

int QnBusinessActionParameters::getParamIndex(const QString& key)
{
    for (int i = 0; i < (int) ParamCount; ++i)
    {
        if (key == PARANAMES[i])
            return i;
    }
    return -1;
}


QnBusinessActionParameters QnBusinessActionParameters::fromBusinessParams(const QnBusinessParams& bParams)
{
    QnBusinessActionParameters result;

    for (QnBusinessParams::const_iterator itr = bParams.begin(); itr != bParams.end(); ++itr)
    {
        int paramIndex = getParamIndex(itr.key());
        switch (Param( paramIndex))
        {
        case SoundUrlParam:
            result.soundUrl = itr.value().toString();
            break;
        case EmailAddressParam:
            result.emailAddress = itr.value().toString();
            break;
        case UserGroupParam:
            result.userGroup = static_cast<UserGroup>(itr.value().toInt());
            break;
        case FpsParam:
            result.fps = itr.value().toInt();
            break;
        case QualityParam:
            result.streamQuality = static_cast<Qn::StreamQuality>(itr.value().toInt());
            break;
        case DurationParam:
            result.recordingDuration = itr.value().toInt();
            break;
        case RecordAfterParam:
            result.recordAfter = itr.value().toInt();
            break;
        case RelayOutputIdParam:
            result.relayOutputId = itr.value().toString();
            break;
        case RelayAutoResetTimeoutParam:
            result.relayAutoResetTimeout = itr.value().toInt();
            break;
        case InputPortIdParam:
            result.inputPortId = itr.value().toString();
            break;
        case KeyParam:
            result.keyParam = itr.value().toString();
            break;
        case SayTextParam:
            result.sayText = itr.value().toString();
            break;
        default:
            break;
        }
    }

    return result;
}

QByteArray serializeBusinessParams(const QnBusinessParams& value) {
    QByteArray result;
    QJson::serialize(QJsonObject::fromVariantMap(value), &result); // TODO: #Elric unuse variants
    return result;
}

QnBusinessParams deserializeBusinessParams(const QByteArray& value) {
    QJsonValue result;
    QJson::deserialize(value, &result); // TODO: #Elric unuse variants
    return result.toObject().toVariantMap(); /* Returns empty map in case of deserialization error. */
}
