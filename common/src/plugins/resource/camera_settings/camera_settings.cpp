#include "camera_settings.h"

#include <cmath>

#include <QtCore/QDebug>
#include <QtCore/QFile>
#include <QtCore/QStringList>

#include <core/resource/resource.h>

//
// CameraSettingValue
//

CameraSettingValue::CameraSettingValue()
{

}

CameraSettingValue::CameraSettingValue(const QString& val):
    value(val)
{

}

CameraSettingValue::CameraSettingValue(const char* val):
    value(QString::fromLatin1(val))

{

}

CameraSettingValue::CameraSettingValue(int val)
{
    value.setNum(val);
}

CameraSettingValue::CameraSettingValue(double val)
{
    value.setNum(val);
}

CameraSettingValue::operator bool() const
{
    return value.toInt();
}

CameraSettingValue::operator int() const
{
    return value.toInt();
}

CameraSettingValue::operator double() const
{
    return value.toDouble();
}

CameraSettingValue::operator QString() const
{
    return value;
}

bool CameraSettingValue::operator < ( const CameraSettingValue & other ) const
{
    return value.toDouble() < (double)other;
}

bool CameraSettingValue::operator > ( const CameraSettingValue & other ) const
{
    return value.toDouble() > (double)other;
}

bool CameraSettingValue::operator <= ( const CameraSettingValue & other ) const
{
    return value.toDouble() <= (double)other;
}

bool CameraSettingValue::operator >= ( const CameraSettingValue & other ) const
{
    return value.toDouble() >= (double)other;
}

bool CameraSettingValue::operator == ( const CameraSettingValue & other ) const
{
    return value == other.value;
}

bool CameraSettingValue::operator == ( int val ) const
{
    return (fabs(value.toDouble() - val)) < 1e-6;
}

bool CameraSettingValue::operator == ( double val ) const
{
    return (fabs(value.toDouble() - val))< 1e-6;
}

bool CameraSettingValue::operator == ( QString val ) const
{
    return value == val;
}

bool CameraSettingValue::operator != ( QString val ) const
{
    return value != val;
}

//
// CameraSetting
//

namespace {
    const QString CAMERA_SETTING_TYPE_ONOFF     = lit("Bool");
    const QString CAMERA_SETTING_TYPE_MINMAX    = lit("MinMaxStep");
    const QString CAMERA_SETTING_TYPE_ENUM      = lit("Enumeration");
    const QString CAMERA_SETTING_TYPE_TEXTFIELD = lit("String");
    const QString CAMERA_SETTING_TYPE_BUTTON    = lit("Button");
    const QString CAMERA_SETTING_TYPE_CTRL_BTNS = lit("ControlButtonsPair");
    const QString CAMERA_SETTING_VAL_ON         = lit("On");
    const QString CAMERA_SETTING_VAL_OFF        = lit("Off");
    const QString SEPARATOR                     = lit(";;");
}

CameraSetting::WIDGET_TYPE CameraSetting::typeFromStr(const QString& value)
{
    if (value == CAMERA_SETTING_TYPE_ONOFF) {
        return OnOff;
    }
    if (value == CAMERA_SETTING_TYPE_MINMAX) {
        return MinMaxStep;
    }
    if (value == CAMERA_SETTING_TYPE_ENUM) {
        return Enumeration;
    }
    if (value == CAMERA_SETTING_TYPE_BUTTON) {
        return Button;
    }
    if (value == CAMERA_SETTING_TYPE_TEXTFIELD) {
        return TextField;
    }
    if (value == CAMERA_SETTING_TYPE_CTRL_BTNS) {
        return ControlButtonsPair;
    }
    return None;
}

QString CameraSetting::strFromType(const WIDGET_TYPE value)
{
    switch (value)
    {
        case OnOff: return CAMERA_SETTING_TYPE_ONOFF;
        case MinMaxStep: return CAMERA_SETTING_TYPE_MINMAX;
        case Enumeration: return CAMERA_SETTING_TYPE_ENUM;
        case Button: return CAMERA_SETTING_TYPE_BUTTON;
        case TextField: return CAMERA_SETTING_TYPE_TEXTFIELD;
        case ControlButtonsPair: return CAMERA_SETTING_TYPE_CTRL_BTNS;
        default: return QString();
    }
}

bool CameraSetting::isTypeWithoutValue(const WIDGET_TYPE value)
{
    return value == Button || value == ControlButtonsPair;
}

CameraSetting::CameraSetting()
:
    m_type(None),
    m_isReadOnly(false)
{
}

CameraSetting::CameraSetting(
    const QString& id,
    const QString& name,
    WIDGET_TYPE type,
    const QString& query,
    const QString& method,
    const QString& description,
    const CameraSettingValue& min,
    const CameraSettingValue& max,
    const CameraSettingValue& step,
    const CameraSettingValue& current,
    bool paramReadOnly )
:
    m_id(id),
    m_name(name),
    m_type(type),
    m_query(query),
    m_method(method),
    m_description(description),
    m_min(min),
    m_max(max),
    m_step(step),
    m_current(current),
    m_isReadOnly(paramReadOnly)
{
    switch (m_type)
    {
        case OnOff:
            //For this type, min and max values are constant
            m_min = CAMERA_SETTING_VAL_OFF;
            m_max = CAMERA_SETTING_VAL_ON;
            break;
        case TextField:
            //For this type, min and max values are not required, so insert dummy values
            m_min = lit("min");
            m_max = lit("max");
            break;
        case ControlButtonsPair:
            //For this type, 'current' is not defined unless user presses min or max buttons
            m_current = m_step;
            break;
        default:
            break;
    }
}

void CameraSetting::setId(const QString& id) {
    m_id = id;
}

QString CameraSetting::getId() const {
    return m_id;
}

void CameraSetting::setName(const QString& name) {
    m_name = name;
}

QString CameraSetting::getName() const {
    return m_name;
}

void CameraSetting::setType(WIDGET_TYPE type) {
    m_type = type;
}

CameraSetting::WIDGET_TYPE CameraSetting::getType() const {
    return m_type;
}

void CameraSetting::setQuery(const QString& query)
{
    m_query = query;
}

QString CameraSetting::getQuery() const
{
    return m_query;
}

void CameraSetting::setMethod(const QString& method)
{
    m_method = method;
}

QString CameraSetting::getMethod() const
{
    return m_method;
}

void CameraSetting::setDescription(const QString& description)
{
    m_description = description;
}

QString CameraSetting::getDescription() const
{
    return m_description;
}

void CameraSetting::setMin(const CameraSettingValue& min) {
    m_min = min;
}

CameraSettingValue CameraSetting::getMin() const {
    return m_min;
}

void CameraSetting::setMax(const CameraSettingValue& max) {
    m_max = max;
}

CameraSettingValue CameraSetting::getMax() const {
    return m_max;
}

void CameraSetting::setStep(const CameraSettingValue& step) {
    m_step = step;
}

CameraSettingValue CameraSetting::getStep() const {
    return m_step;
}

void CameraSetting::setCurrent(const CameraSettingValue& current) {
    m_current = current;
}

CameraSettingValue CameraSetting::getCurrent() const {
    return m_current;
}

void CameraSetting::setIsReadOnly( bool val )
{
    m_isReadOnly = val;
}

bool CameraSetting::isReadOnly() const
{
    return m_isReadOnly;
}

CameraSetting& CameraSetting::operator= (const CameraSetting& rhs)
{
    setId(rhs.getId());
    setName(rhs.getName());
    setType(rhs.getType());
    setQuery(rhs.getQuery());
    setMethod(rhs.getMethod());
    setDescription(rhs.getDescription());
    setMin(rhs.getMin());
    setMax(rhs.getMax());
    setStep(rhs.getStep());
    setCurrent(rhs.getCurrent());

    return *this;
}

QString CameraSetting::serializeToStr() const
{
    QString result;

    result.append(m_id);

    result.append(SEPARATOR);
    result.append(m_name);

    result.append(SEPARATOR);
    result.append(strFromType(m_type));

    result.append(SEPARATOR);
    result.append(static_cast<QString>(m_min));

    result.append(SEPARATOR);
    result.append(static_cast<QString>(m_max));

    result.append(SEPARATOR);
    result.append(static_cast<QString>(m_step));

    result.append(SEPARATOR);
    result.append(static_cast<QString>(m_current));

    return result;
}

bool CameraSetting::deserializeFromStr(const QString& src)
{
    QStringList tokens = src.split(SEPARATOR);
    if (tokens.length() != 7) {
        qWarning() << "CameraSetting::deserializeFromStr: unexpected serialized data: " << src;
        return false;
    }

    m_id = tokens[0];
    m_name = tokens[1];
    m_type = typeFromStr(tokens[2]);
    m_min = CameraSettingValue(tokens[3]);
    m_max = CameraSettingValue(tokens[4]);
    m_step = CameraSettingValue(tokens[5]);
    m_current = CameraSettingValue(tokens[6]);

    return true;
}

bool CameraSetting::isDisabled() const
{
    return static_cast<QString>(m_current).isEmpty() ||
        (m_type != Enumeration && m_min == m_max);
}

//
// class CameraSettingReader
//

const QString ID_SEPARATOR(QLatin1String("%%"));
const QString TAG_GROUP(QLatin1String("group"));
const QString TAG_PARAM(QLatin1String("param"));
const QString ATTR_PARENT(QLatin1String("parent"));
const QString ATTR_ID(QLatin1String("id"));
const QString ATTR_NAME(QLatin1String("name"));
const QString ATTR_DATA_TYPE(QLatin1String("dataType"));
const QString ATTR_QUERY(QLatin1String("query"));
const QString ATTR_METHOD(QLatin1String("method"));
const QString ATTR_DESCRIPTION(QLatin1String("description"));
const QString ATTR_MIN(QLatin1String("min"));
const QString ATTR_MAX(QLatin1String("max"));
const QString ATTR_STEP(QLatin1String("step"));
const QString ATTR_READ_ONLY(QLatin1String("readOnly"));
const QString CSR_CAMERA_SETTINGS_FILEPATH(QLatin1String(":/camera_settings/camera_settings.xml"));

QString CameraSettingReader::createId(const QString& parentId, const QString& name)
{
    return parentId + ID_SEPARATOR + name;
}

bool CameraSettingReader::isEnabled(const CameraSetting& val)
{
    return !static_cast<QString>(val.getCurrent()).isEmpty() &&
        (val.getType() == CameraSetting::Enumeration || val.getMin() != val.getMax());
}

CameraSettingReader::CameraSettingReader(
    const QString& cameraId,
    const QnResourcePtr& cameraRes )
:
    m_filepath(CSR_CAMERA_SETTINGS_FILEPATH),
    m_cameraId(cameraId),
    m_cameraRes(cameraRes)
{
}

CameraSettingReader::~CameraSettingReader()
{
}

void CameraSettingReader::setCamera( const QnResourcePtr& cameraRes )
{
    m_cameraRes = cameraRes;
}

bool CameraSettingReader::read()
{
    std::unique_ptr<QIODevice> dataSource;

    if( m_cameraRes )
    {
        const QString& cameraSettingsXml = m_cameraRes->getProperty( Qn::PHYSICAL_CAMERA_SETTINGS_XML_PARAM_NAME );
        if( !cameraSettingsXml.isEmpty() )
        {
            dataSource.reset( new QBuffer() );
            static_cast<QBuffer*>(dataSource.get())->setData( cameraSettingsXml.toUtf8() );
        }
    }
    
    if( !dataSource )
    {
        dataSource.reset( new QFile(m_filepath) );
        if (!static_cast<QFile*>(dataSource.get())->exists())
        {
            qWarning() << "Cannot open file '" << m_filepath << "'";
            return false;
        }
    }

    QString errorStr;
    int errorLine;
    int errorColumn;

    if (!m_doc.setContent(dataSource.get(), &errorStr, &errorLine, &errorColumn))
    {
        qWarning() << "File '" << m_filepath << "'. Parse error at line: " << errorLine << ", column: " << errorColumn << ", error: " << errorStr;
        return false;
    }

    return true;
}

bool CameraSettingReader::proceed()
{
    QDomElement root = m_doc.documentElement();
    if (root.tagName() != QLatin1String("cameras"))
        return false;

    QDomNode node = root.firstChild();

    for (; !node.isNull(); node = node.nextSibling())
    {
        if (node.toElement().tagName() == QLatin1String("camera"))
        {
            const QString& foundCameraName = node.attributes().namedItem(ATTR_NAME).nodeValue();
            if( !m_cameraId.isEmpty() &&    //do not check camera id
                (foundCameraName != m_cameraId) )
            {
                continue;
            }

            QString parentId = node.attributes().namedItem(ATTR_PARENT).nodeValue();
            if (!parentId.isEmpty()) {
                parentOfRootElemFound( parentId );
            }
            cameraElementFound( foundCameraName, parentId );

            return parseCameraXml(node.toElement());
        }
    }

    qWarning() << "File '" << m_filepath << "'. Can't find camera: " << m_cameraId;

    return true;
}

bool CameraSettingReader::parseCameraXml(const QDomElement& cameraXml)
{
    if (!parseGroupXml(cameraXml.firstChildElement(), QLatin1String(""))) {
        cleanDataOnFail();
        return false;
    }

    return true;
}

bool CameraSettingReader::parseGroupXml(const QDomElement& groupXml, const QString parentId)
{
    if (groupXml.isNull()) {
        return true;
    }

    QString tagName = groupXml.nodeName();

    if (tagName == TAG_PARAM && !parseElementXml(groupXml, parentId)) {
        return false;
    }

    if (tagName == TAG_GROUP)
    {
        QString name = groupXml.attribute(ATTR_NAME);
        if (name.isEmpty()) {
            qWarning() << "File '" << m_filepath << "' has tag '" << TAG_GROUP << "' w/o attribute '" << ATTR_NAME << "'";
            return false;
        }
        QString id = createId(parentId, name);

        if (isGroupEnabled(id, parentId, name) && !parseGroupXml(groupXml.firstChildElement(), id))
        {
            return false;
        }
    }

    if (!parseGroupXml(groupXml.nextSiblingElement(), parentId)) {
        return false;
    }

    return true;
}

bool CameraSettingReader::parseElementXml(const QDomElement& elementXml, const QString parentId)
{
    if (elementXml.isNull() || elementXml.nodeName() != TAG_PARAM) {
        return true;
    }

    QString name = elementXml.attribute(ATTR_NAME);
    if (name.isEmpty()) {
        qWarning() << "File '" << m_filepath << "' has tag '" << TAG_PARAM << "' w/o attribute '" << ATTR_NAME << "'";
        return false;
    }

    QString id = createId(parentId, name);

    if (!isParamEnabled(id, parentId)) {
        return true;
    }

    QString widgetTypeStr = elementXml.attribute(ATTR_DATA_TYPE);
    if (widgetTypeStr.isEmpty()) {
        qWarning() << "File '" << m_filepath << "' has tag '" << TAG_PARAM << "' w/o attribute '" << ATTR_DATA_TYPE << "'";
        return false;
    }
    CameraSetting::WIDGET_TYPE widgetType = CameraSetting::typeFromStr(widgetTypeStr);

    QString query = elementXml.attribute(ATTR_QUERY);
    QString method = elementXml.attribute(ATTR_METHOD);
    QString description = elementXml.attribute(ATTR_DESCRIPTION);
    CameraSettingValue min = CameraSettingValue(elementXml.attribute(ATTR_MIN));
    CameraSettingValue max = CameraSettingValue(elementXml.attribute(ATTR_MAX));
    CameraSettingValue step = CameraSettingValue(elementXml.attribute(ATTR_STEP));
    const bool paramReadOnly = elementXml.attribute(ATTR_READ_ONLY, lit("false")) == lit("true");

    switch(widgetType)
    {
        case CameraSetting::OnOff:
        case CameraSetting::MinMaxStep:
        case CameraSetting::Enumeration:
        case CameraSetting::Button:
        case CameraSetting::TextField:
        case CameraSetting::ControlButtonsPair:
            paramFound(CameraSetting(id, name, widgetType, query, method, description, min, max, step, CameraSettingValue(), paramReadOnly), parentId);
            return true;

        default:
            qWarning() << "File '" << m_filepath << "' has tag '" << TAG_PARAM << "' has unexpected value of attribute '"
                << ATTR_DATA_TYPE << "': " << widgetTypeStr;
            return false;
    }

    return true;
}
