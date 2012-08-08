#ifndef camera_settings_h_1726
#define camera_settings_h_1726

class CameraSettingValue
{
    QString value;
public:
    CameraSettingValue();
    CameraSettingValue(const QString& val);
    CameraSettingValue(const char* val);
    CameraSettingValue(int val);
    CameraSettingValue(double val);

    operator bool() const;
    operator int() const;
    operator double() const;
    operator QString() const;

    bool operator < ( const CameraSettingValue & other ) const;
    bool operator > ( const CameraSettingValue & other ) const;
    bool operator <= ( const CameraSettingValue & other ) const;
    bool operator >= ( const CameraSettingValue & other ) const;
    bool operator == ( const CameraSettingValue & other ) const;
    bool operator == ( int val ) const;
    bool operator == ( double val ) const;
    bool operator == ( QString val ) const;
    bool operator != ( QString val ) const;
};

class CameraSetting {
public:
    enum WIDGET_TYPE { None, Value, OnOff, Boolean, MinMaxStep, Enumeration, Button };

    static WIDGET_TYPE typeFromStr(const QString& value);

    static QString strFromType(const WIDGET_TYPE value);

    static QString& SEPARATOR;

    CameraSetting() {}

    CameraSetting(const QString& id,
        const QString& name,
        WIDGET_TYPE type,
        const CameraSettingValue min = CameraSettingValue(),
        const CameraSettingValue max = CameraSettingValue(),
        const CameraSettingValue step = CameraSettingValue(),
        const CameraSettingValue current = CameraSettingValue());

    virtual ~CameraSetting() {};

    void setId(const QString& id);
    QString getId() const;

    void setName(const QString& name);
    QString getName() const;

    void setType(WIDGET_TYPE type);
    WIDGET_TYPE getType() const;

    void setMin(const CameraSettingValue& min);
    CameraSettingValue getMin() const;

    void setMax(const CameraSettingValue& max);
    CameraSettingValue getMax() const;

    void setStep(const CameraSettingValue& step);
    CameraSettingValue getStep() const;

    void setCurrent(const CameraSettingValue& current);
    CameraSettingValue getCurrent() const;

    CameraSetting& operator= (const CameraSetting& rhs);

    QString serializeToStr() const;
    void deserializeFromStr(const QString& src);

    bool isDisabled() const;

private:
    QString m_id;
    QString m_name;
    WIDGET_TYPE m_type;
    CameraSettingValue m_min;
    CameraSettingValue m_max;
    CameraSettingValue m_step;
    CameraSettingValue m_current;
};

#endif //camera_settings_h_1726
