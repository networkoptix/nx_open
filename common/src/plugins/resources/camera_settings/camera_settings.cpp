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

CameraSetting::WIDGET_TYPE CameraSetting::typeFromStr(const QString& value)
{
    if (value == QLatin1String("Value")) {
        return Value;
    }
    if (value == QLatin1String("OnOff")) {
        return OnOff;
    }
    if (value == QLatin1String("Boolean")) {
        return Boolean;
    }
    if (value == QLatin1String("MinMaxStep")) {
        return MinMaxStep;
    }
    if (value == QLatin1String("Enumeration")) {
        return Enumeration;
    }
    if (value == QLatin1String("Button")) {
        return Button;
    }
    return None;
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
