#ifndef QN_PARAM_H
#define QN_PARAM_H

#include <QtCore/QHash>
#include <QtCore/QList>
#include <QtCore/QSharedPointer>
#include <QtCore/QVariant>

#include <utils/common/id.h>

#include "resource_fwd.h"


namespace Qn
{
    //dynamic parameters of resource

    static const QString HAS_DUAL_STREAMING_PARAM_NAME = lit("hasDualStreaming");
    static const QString DTS_PARAM_NAME = lit("dts");
    static const QString ANALOG_PARAM_NAME = lit("analog");
    static const QString IS_AUDIO_SUPPORTED_PARAM_NAME = lit("isAudioSupported");
    static const QString STREAM_FPS_SHARING_PARAM_NAME = lit("streamFpsSharing");
    static const QString MAX_FPS_PARAM_NAME = QLatin1String("MaxFPS");
    static const QString FORCED_AUDIO_SUPPORTED_PARAM_NAME = lit("forcedIsAudioSupported");
    static const QString MOTION_WINDOW_CNT_PARAM_NAME = lit("motionWindowCnt");
    static const QString MOTION_MASK_WINDOW_CNT_PARAM_NAME = lit("motionMaskWindowCnt");
    static const QString MOTION_SENS_WINDOW_CNT_PARAM_NAME = lit("motionSensWindowCnt");
    static const QString FORCED_IS_AUDIO_SUPPORTED_PARAM_NAME = lit("forcedIsAudioSupported");
    //!possible values: softwaregrid,hardwaregrid
    static const QString SUPPORTED_MOTION_PARAM_NAME = lit("supportedMotion");
    static const QString CAMERA_CAPABILITIES_PARAM_NAME = lit("cameraCapabilities");
    static const QString CAMERA_MEDIA_STREAM_LIST_PARAM_NAME = lit("mediaStreams");

}

// TODO: #Elric #enum
enum QnDomain
{
    QnDomainMemory = 1,
    QnDomainDatabase = 2,
    QnDomainPhysical = 4
};

struct QN_EXPORT QnParamType
{

    QnParamType();
    //QnParamType(const QString& name, const QVariant &val);

    Qn::PropertyDataType type;

    //QnUuid id;
    QString name;
    QString default_value;

    double min_val;
    double max_val;
    double step;

    QList<QVariant> possible_values;
    QList<QVariant> ui_possible_values;

    QString paramNetHelper;
    QString group;
    QString subgroup;
    QString description;

    bool ui;
    bool isReadOnly;
    bool isPhysical;
};


struct QN_EXPORT QnParam
{
    QnParam(): m_paramType(new QnParamType), m_domain(QnDomainMemory) {}

    QnParam(QnParamTypePtr paramType, const QVariant &value = QVariant(), QnDomain domain = QnDomainMemory): m_paramType(paramType), m_domain(domain) { 
        setValue(value); 
    }

    bool isValid() const { return !m_paramType->name.isEmpty(); }

    bool setValue(const QVariant &val); // safe way to set value
    const QVariant &value() const { return m_value; }
    QnDomain domain() const { return m_domain; }
    void setDomain(QnDomain domain) { m_domain = domain; }
    const QString &defaultValue() const { return m_paramType->default_value; }
    const QString &name() const { return m_paramType->name; }
    const QString &group() const { return m_paramType->group; }
    const QString &subGroup() const { return m_paramType->subgroup; }
    double minValue() const { return m_paramType->min_val; }
    double maxValue() const { return m_paramType->max_val; }
    Qn::PropertyDataType type() const { return m_paramType->type;}
    const QList<QVariant> &uiPossibleValues() const { return m_paramType->ui_possible_values; }
    const QList<QVariant> &possibleValues() const { return m_paramType->possible_values; }
    const QString &description() const { return m_paramType->description; }
    bool isUiParam() const { return m_paramType->ui; }
    bool isPhysical() const { return m_paramType->isPhysical; }
    bool isReadOnly() const { return m_paramType->isReadOnly; }
    const QString &netHelper() const { return m_paramType->paramNetHelper; }
    //const QnUuid &paramTypeId() const { return m_paramType->id; }

private:
    QnParamTypePtr m_paramType;
    QVariant m_value;
    QnDomain m_domain;
};

Q_DECLARE_METATYPE(QnParam)

typedef QHash<QString, QnParam> QnParamList;

#endif // QN_PARAM_H
