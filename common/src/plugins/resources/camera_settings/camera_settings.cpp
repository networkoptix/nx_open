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
        const CameraSettingValue min, const CameraSettingValue max,
        const CameraSettingValue step, const CameraSettingValue current) :
    m_id(id),
    m_name(name),
    m_type(type),
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

void CameraSetting::deserializeFromStr(const QString& src)
{
    QStringList tokens = src.split(SEPARATOR);
    if (tokens.length() != 7) {
        Q_ASSERT(false);
    }

    m_id = tokens[0];
    m_name = tokens[1];
    m_type = typeFromStr(tokens[2]);
    m_min = CameraSettingValue(tokens[3]);
    m_max = CameraSettingValue(tokens[4]);
    m_step = CameraSettingValue(tokens[5]);
    m_current = CameraSettingValue(tokens[6]);
}

bool CameraSetting::isDisabled() const
{
    return static_cast<QString>(m_current).isEmpty() ||
        m_type != Enumeration && m_min == m_max;
}
