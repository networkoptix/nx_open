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

    CameraSetting(const QString& id,
        const QString& name,
        WIDGET_TYPE type,
        const CameraSettingValue min = CameraSettingValue(),
        const CameraSettingValue max = CameraSettingValue(),
        const CameraSettingValue step = CameraSettingValue(),
        const CameraSettingValue current = CameraSettingValue());

    void setId(const QString& id);
    QString getId();

    void setName(const QString& name);
    QString getName();

    void setType(WIDGET_TYPE type);
    WIDGET_TYPE getType();

    void setMin(const CameraSettingValue& min);
    CameraSettingValue getMin();

    void setMax(const CameraSettingValue& max);
    CameraSettingValue getMax();

    void setStep(const CameraSettingValue& step);
    CameraSettingValue getStep();

    void setCurrent(const CameraSettingValue& current);
    CameraSettingValue getCurrent();

protected:
    virtual ~CameraSetting() {};

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
