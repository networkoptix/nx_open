#include "camera_settings.h"

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
        default: return QString();
    }
}

CameraSetting::CameraSetting(const QString& id, const QString& name, WIDGET_TYPE type,
        const QString& query, const CameraSettingValue min, const CameraSettingValue max,
        const CameraSettingValue step, const CameraSettingValue current) :
    m_id(id),
    m_name(name),
    m_type(type),
    m_query(query),
    m_min(min),
    m_max(max),
    m_step(step),
    m_current(current)
{}

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
        m_type != Enumeration && m_min == m_max;
}

//
// class CameraSettingReader
//

const QString& CameraSettingReader::ID_SEPARATOR = *(new QString(QLatin1String("%%")));
const QString& CameraSettingReader::TAG_GROUP = *(new QString(QLatin1String("group")));
const QString& CameraSettingReader::TAG_PARAM = *(new QString(QLatin1String("param")));
const QString& CameraSettingReader::ATTR_ID = *(new QString(QLatin1String("id")));
const QString& CameraSettingReader::ATTR_NAME = *(new QString(QLatin1String("name")));
const QString& CameraSettingReader::ATTR_WIDGET_TYPE = *(new QString(QLatin1String("widget_type")));
const QString& CameraSettingReader::ATTR_QUERY = *(new QString(QLatin1String("query")));
const QString& CameraSettingReader::ATTR_MIN = *(new QString(QLatin1String("min")));
const QString& CameraSettingReader::ATTR_MAX = *(new QString(QLatin1String("max")));
const QString& CameraSettingReader::ATTR_STEP = *(new QString(QLatin1String("step")));

QString CameraSettingReader::createId(const QString& parentId, const QString& name)
{
    return parentId + ID_SEPARATOR + name;
}

bool CameraSettingReader::isEnabled(const CameraSetting& val)
{
    return !static_cast<QString>(val.getCurrent()).isEmpty() &&
        (val.getType() == CameraSetting::Enumeration || val.getMin() != val.getMax());
}

CameraSettingReader::CameraSettingReader(const QString& filepath, const QString& cameraId) :
    m_filepath(filepath),
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

    while (!node.isNull())
    {
        if (node.toElement().tagName() == QLatin1String("camera"))
        {
            if (node.attributes().namedItem(ATTR_NAME).nodeValue() != QLatin1String("ONVIF")) {
                continue;
            }
            return parseCameraXml(node.toElement());
        }

        node = node.nextSibling();
    }

    qWarning() << "File '" << m_filepath << "'. Can't find camera: " << m_cameraId;

    return true;
}

/*void CameraSettingReader::groupFound(const CameraSetting& value)
{
    QString id = value.getId();
    CameraSettingsByIds::ConstIterator it = m_settingsByIds.find(id);

    if (it != m_settingsByIds.end()) {
        //id must be unique
        Q_ASSERT(false);
    }

    m_settingsByIds.insert(id, value);
}

void CameraSettingReader::paramFound(const CameraSetting& value)
{
    QString id = value.getId();
    CameraSettingsByIds::ConstIterator it = m_settingsByIds.find(id);

    if (it != m_settingsByIds.end()) {
        //id must be unique
        Q_ASSERT(false);
    }

    m_settingsByIds.insert(id, value);
}*/

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
    CameraSettingValue min = CameraSettingValue(elementXml.attribute(ATTR_MIN));
    CameraSettingValue max = CameraSettingValue(elementXml.attribute(ATTR_MAX));
    CameraSettingValue step = CameraSettingValue(elementXml.attribute(ATTR_STEP));

    switch(widgetType)
    {
    case CameraSetting::OnOff: case CameraSetting::MinMaxStep: case CameraSetting::Enumeration: case CameraSetting::Button:
        paramFound(CameraSetting(id, name, widgetType, min, max, step), parentId);
        return true;

    default:
        qWarning() << "File '" << m_filepath << "' has tag '" << TAG_PARAM << "' has unexpected value of attribute '"
            << ATTR_WIDGET_TYPE << "': " << widgetTypeStr;
        return false;
    }

    return true;
}
