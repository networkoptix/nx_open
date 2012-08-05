#ifndef camera_settings_widgets_h_1214
#define camera_settings_widgets_h_1214

#include <QtGui/QSlider>

//#include "device/param.h"
//#include "base/log.h"
//class QnDevice;
class QGroupBox;


class CameraSettingValue
{
    QString value;
public:
    CameraSettingValue(){}
    CameraSettingValue(const QString& val) : value(val){}
    CameraSettingValue(const char* val) : value(QString::fromLatin1(val)){}

    CameraSettingValue(int val)
    {
        value.setNum(val);
    }

    CameraSettingValue(double val)
    {
        value.setNum(val);
    }

    operator bool() const { return value.toInt();}
    operator int() const { return value.toInt(); }
    operator double() const { return value.toDouble(); }
    operator QString() const { return value; }

    bool operator < ( const CameraSettingValue & other ) const
    {
        return value.toDouble() < (double)other;
    }

    bool operator > ( const CameraSettingValue & other ) const
    {
        return value.toDouble() > (double)other;
    }

    bool operator <= ( const CameraSettingValue & other ) const
    {
        return value.toDouble() <= (double)other;
    }

    bool operator >= ( const CameraSettingValue & other ) const
    {
        return value.toDouble() >= (double)other;
    }

    bool operator == ( const CameraSettingValue & other ) const
    {
        return value == other.value;
    }

    bool operator == ( int val ) const
    {
        return (fabs(value.toDouble() - val)) < 1e-6;
    }

    bool operator == ( double val ) const
    {
        return (fabs(value.toDouble() - val))< 1e-6;
    }

    bool operator == ( QString val ) const
    {
        return value == val;
    }

    bool operator != ( QString val ) const
    {
        return value != val;
    }

};

class CameraSetting {
public:
    enum WIDGET_TYPE { None, Value, OnOff, Boolean, MinMaxStep, Enumeration, Button };

    static WIDGET_TYPE typeFromStr(const QString& value)
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

    CameraSetting(const QString& id,
        const QString& name,
        WIDGET_TYPE type,
        const CameraSettingValue min = CameraSettingValue(),
        const CameraSettingValue max = CameraSettingValue(),
        const CameraSettingValue step = CameraSettingValue(),
        const CameraSettingValue current = CameraSettingValue()) :
    m_id(id),
        m_name(name),
        m_type(type),
        m_min(min),
        m_max(max),
        m_step(step),
        m_current(current)
    {}

    void setId(const QString& id) {
        m_id = id;
    }

    QString getId() {
        return m_id;
    }

    void setName(const QString& name) {
        m_name = name;
    }

    QString getName() {
        return m_name;
    }

    void setType(WIDGET_TYPE type) {
        m_type = type;
    }

    WIDGET_TYPE getType() {
        return m_type;
    }

    void setMin(const CameraSettingValue& min) {
        m_min = min;
    }

    CameraSettingValue getMin() {
        return m_min;
    }

    void setMax(const CameraSettingValue& max) {
        m_max = max;
    }

    CameraSettingValue getMax() {
        return m_max;
    }

    void setStep(const CameraSettingValue& step) {
        m_step = step;
    }

    CameraSettingValue getStep() {
        return m_step;
    }

    void setCurrent(const CameraSettingValue& current) {
        m_current = current;
    }

    CameraSettingValue getCurrent() {
        return m_current;
    }

private:
    QString m_id;
    QString m_name;
    WIDGET_TYPE m_type;
    CameraSettingValue m_min;
    CameraSettingValue m_max;
    CameraSettingValue m_step;
    CameraSettingValue m_current;

    CameraSetting();
};

typedef QSharedPointer<QWidget> QWidgetPtr;
typedef QHash<QString, CameraSetting> CameraSettings;
typedef QHash<QString, QWidgetPtr> WidgetsById;

// ****************************************************** //

class QnSettingsSlider : public QSlider
{
    Q_OBJECT

public:
    QnSettingsSlider(Qt::Orientation orientation, QWidget *parent = 0);

Q_SIGNALS:
    void onKeyReleased();

protected:
    void keyReleaseEvent(QKeyEvent *event);
};

//==============================================

class QnAbstractSettingsWidget : public QWidget //public QObject
{
    Q_OBJECT
public:
    QnAbstractSettingsWidget(QObject* handler, CameraSetting& obj);
    virtual ~QnAbstractSettingsWidget();

    virtual QWidget* toWidget();

    const CameraSetting& param() const;

signals:
    void setParam(const QString& name, const CameraSettingValue& val);

    public slots:
        virtual void updateParam(QString val) = 0;

protected:
    virtual void setParam_helper(const QString& name, const CameraSettingValue& val);
protected:
    CameraSetting& mParam;
    QObject* mHandler;
    QWidget* mWidget;
};
//==============================================
class QnSettingsOnOffWidget : public QnAbstractSettingsWidget
{
    Q_OBJECT
public:
    QnSettingsOnOffWidget(QObject* handler, CameraSetting& obj, QWidget& parent);
    ~QnSettingsOnOffWidget();

    public slots:
        void updateParam(QString val);

        private slots:
            void stateChanged (int state);
private:
    QCheckBox * m_checkBox;
};
//==============================================
class QnSettingsMinMaxStepWidget : public QnAbstractSettingsWidget
{
    Q_OBJECT
public:
    QnSettingsMinMaxStepWidget(QObject* handler, CameraSetting& obj, QWidget& parent);

    public slots:
        void updateParam(QString val);

        private slots:
            void onValChanged();
            void onValChanged(int val);

private:
    QnSettingsSlider* m_slider;
    QGroupBox* groupBox;
};
//==============================================
class QnSettingsEnumerationWidget : public QnAbstractSettingsWidget
{
    Q_OBJECT
public:
    QnSettingsEnumerationWidget(QObject* handler, CameraSetting& obj, QWidget& parent);

    public slots:
        void updateParam(QString val);

        private slots:
            void onClicked();
private:
    QRadioButton* getBtnByname(const QString& name);
private:
    QList<QRadioButton*> m_radioBtns;
};

//==============================================
class QnSettingsButtonWidget : public QnAbstractSettingsWidget
{
    Q_OBJECT
public:
    QnSettingsButtonWidget(QObject* handler, CameraSetting& obj, QWidget& parent);

    public slots:
        void updateParam(QString val);

        private slots:
            void onClicked();

};

#endif //camera_settings_widgets_h_1214
