#include "camera_settings.h"

#include <cmath>

#include <QtCore/QDebug>
#include <QtCore/QFile>
#include <QtCore/QStringList>

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

QString& CAMERA_SETTING_TYPE_VALUE  = *(new QString(QLatin1String("Value")));
QString& CAMERA_SETTING_TYPE_ONOFF  = *(new QString(QLatin1String("OnOff")));
QString& CAMERA_SETTING_TYPE_BOOL   = *(new QString(QLatin1String("Boolean")));
QString& CAMERA_SETTING_TYPE_MINMAX = *(new QString(QLatin1String("MinMaxStep")));
QString& CAMERA_SETTING_TYPE_ENUM   = *(new QString(QLatin1String("Enumeration")));
QString& CAMERA_SETTING_TYPE_BUTTON = *(new QString(QLatin1String("Button")));
QString& CAMERA_SETTING_TYPE_TEXTFIELD = *(new QString(QLatin1String("TextField")));
QString& CAMERA_SETTING_TYPE_CTRL_BTNS = *(new QString(QLatin1String("ControlButtonsPair")));
QString& CAMERA_SETTING_VAL_ON = *(new QString(QLatin1String("On")));
QString& CAMERA_SETTING_VAL_OFF = *(new QString(QLatin1String("Off")));

QString& CameraSetting::SEPARATOR = *(new QString(QLatin1String(";;")));

CameraSetting::WIDGET_TYPE CameraSetting::typeFromStr(const QString& value)
{
    if (value == CAMERA_SETTING_TYPE_VALUE) {
        return Value;
    }
    if (value == CAMERA_SETTING_TYPE_ONOFF) {
        return OnOff;
    }
    if (value == CAMERA_SETTING_TYPE_BOOL) {
        return Boolean;
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
        case Value: return CAMERA_SETTING_TYPE_VALUE;
        case OnOff: return CAMERA_SETTING_TYPE_ONOFF;
        case Boolean: return CAMERA_SETTING_TYPE_BOOL;
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

CameraSetting::CameraSetting(const QString& id, const QString& name, WIDGET_TYPE type,
        const QString& query, const QString& method, const QString& description, const CameraSettingValue min,
        const CameraSettingValue max, const CameraSettingValue step, const CameraSettingValue current) :
    m_id(id),
    m_name(name),
    m_type(type),
    m_query(query),
    m_method(method),
    m_description(description),
    m_min(min),
    m_max(max),
    m_step(step),
    m_current(current)
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

const QString& CameraSettingReader::ID_SEPARATOR = *(new QString(QLatin1String("%%")));
const QString& CameraSettingReader::TAG_GROUP = *(new QString(QLatin1String("group")));
const QString& CameraSettingReader::TAG_PARAM = *(new QString(QLatin1String("param")));
const QString& CameraSettingReader::ATTR_PARENT = *(new QString(QLatin1String("parent")));
const QString& CameraSettingReader::ATTR_ID = *(new QString(QLatin1String("id")));
const QString& CameraSettingReader::ATTR_NAME = *(new QString(QLatin1String("name")));
const QString& CameraSettingReader::ATTR_WIDGET_TYPE = *(new QString(QLatin1String("widget_type")));
const QString& CameraSettingReader::ATTR_QUERY = *(new QString(QLatin1String("query")));
const QString& CameraSettingReader::ATTR_METHOD = *(new QString(QLatin1String("method")));
const QString& CameraSettingReader::ATTR_DESCRIPTION = *(new QString(QLatin1String("description")));
const QString& CameraSettingReader::ATTR_MIN = *(new QString(QLatin1String("min")));
const QString& CameraSettingReader::ATTR_MAX = *(new QString(QLatin1String("max")));
const QString& CameraSettingReader::ATTR_STEP = *(new QString(QLatin1String("step")));
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

CameraSettingReader::CameraSettingReader(const QString& cameraId) :
    m_filepath(CSR_CAMERA_SETTINGS_FILEPATH),
    m_cameraId(cameraId)
{
}

CameraSettingReader::~CameraSettingReader()
{

}

bool CameraSettingReader::read()
{
    QFile file(m_filepath);

    if (!file.exists())
    {
        qWarning() << "Cannot open file '" << m_filepath << "'";
        return false;
    }

    QString errorStr;
    int errorLine;
    int errorColumn;

    if (!m_doc.setContent(&file, &errorStr, &errorLine, &errorColumn))
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
            if (node.attributes().namedItem(ATTR_NAME).nodeValue() != m_cameraId) {
                continue;
            }

            QString parentId = node.attributes().namedItem(ATTR_PARENT).nodeValue();
            if (!parentId.isEmpty()) {
                parentOfRootElemFound(parentId);
            }

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

    QString widgetTypeStr = elementXml.attribute(ATTR_WIDGET_TYPE);
    if (widgetTypeStr.isEmpty()) {
        qWarning() << "File '" << m_filepath << "' has tag '" << TAG_PARAM << "' w/o attribute '" << ATTR_WIDGET_TYPE << "'";
        return false;
    }
    CameraSetting::WIDGET_TYPE widgetType = CameraSetting::typeFromStr(widgetTypeStr);

    QString query = elementXml.attribute(ATTR_QUERY);
    QString method = elementXml.attribute(ATTR_METHOD);
    QString description = elementXml.attribute(ATTR_DESCRIPTION);
    CameraSettingValue min = CameraSettingValue(elementXml.attribute(ATTR_MIN));
    CameraSettingValue max = CameraSettingValue(elementXml.attribute(ATTR_MAX));
    CameraSettingValue step = CameraSettingValue(elementXml.attribute(ATTR_STEP));

    switch(widgetType)
    {
    case CameraSetting::OnOff: case CameraSetting::MinMaxStep: case CameraSetting::Enumeration: case CameraSetting::Button:
    case CameraSetting::TextField: case CameraSetting::ControlButtonsPair:
        paramFound(CameraSetting(id, name, widgetType, query, method, description, min, max, step), parentId);
        return true;

    default:
        qWarning() << "File '" << m_filepath << "' has tag '" << TAG_PARAM << "' has unexpected value of attribute '"
            << ATTR_WIDGET_TYPE << "': " << widgetTypeStr;
        return false;
    }

    return true;
}
