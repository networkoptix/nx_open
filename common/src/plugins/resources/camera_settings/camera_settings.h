#ifndef camera_settings_h_1726
#define camera_settings_h_1726

#include "qdom.h"
#include <QHash>

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
    enum WIDGET_TYPE { None, Value, OnOff, Boolean, MinMaxStep, Enumeration, Button, TextField, ControlButtonsPair };

    static WIDGET_TYPE typeFromStr(const QString& value);
    static QString strFromType(const WIDGET_TYPE value);
    static bool isTypeWithoutValue(const WIDGET_TYPE value);

    static QString& SEPARATOR;

    CameraSetting() {}

    CameraSetting(const QString& id,
        const QString& name,
        WIDGET_TYPE type,
        const QString& query,
        const QString& method,
        const QString& description,
        const CameraSettingValue min = CameraSettingValue(),
        const CameraSettingValue max = CameraSettingValue(),
        const CameraSettingValue step = CameraSettingValue(),
        const CameraSettingValue current = CameraSettingValue());

    virtual ~CameraSetting() {}

    void setId(const QString& id);
    QString getId() const;

    void setName(const QString& name);
    QString getName() const;

    void setType(WIDGET_TYPE type);
    WIDGET_TYPE getType() const;

    void setQuery(const QString& query);
    QString getQuery() const;

    void setMethod(const QString& query);
    QString getMethod() const;

    void setDescription(const QString& description);
    QString getDescription() const;

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
    bool deserializeFromStr(const QString& src);

    bool isDisabled() const;

private:
    QString m_id;
    QString m_name;
    WIDGET_TYPE m_type;
    QString m_query;
    QString m_method;
    QString m_description;
    CameraSettingValue m_min;
    CameraSettingValue m_max;
    CameraSettingValue m_step;
    CameraSettingValue m_current;
};

typedef QHash<QString, CameraSetting> CameraSettingsByIds;

class CameraSettingReader
{
    QDomDocument m_doc;
    QString m_filepath;
    QString m_cameraId;

public:

    static const QString& ID_SEPARATOR;
    static const QString& TAG_GROUP;
    static const QString& TAG_PARAM;
    static const QString& ATTR_PARENT;
    static const QString& ATTR_ID;
    static const QString& ATTR_NAME;
    static const QString& ATTR_WIDGET_TYPE;
    static const QString& ATTR_QUERY;
    static const QString& ATTR_METHOD;
    static const QString& ATTR_DESCRIPTION;
    static const QString& ATTR_MIN;
    static const QString& ATTR_MAX;
    static const QString& ATTR_STEP;

    static QString createId(const QString& parentId, const QString& name);
    static bool isEnabled(const CameraSetting& val);

    CameraSettingReader(const QString& cameraId);
    virtual ~CameraSettingReader();

    bool read(); //reads data from xml file
    bool proceed(); //parses data from file
    QString getCameraId() const { return m_cameraId; }

protected:

    //If 'isGroupEnabled' returns 'true' the reader will enter it, otherwise the group will be ignored.
    virtual bool isGroupEnabled(const QString& id, const QString& parentId, const QString& name) = 0;

    //If 'isParamEnabled' returns 'true' the reader will create it, otherwise the param will be ignored.
    virtual bool isParamEnabled(const QString& id, const QString& parentId) = 0;

    //The reader sends created param to the child
    virtual void paramFound(const CameraSetting& value, const QString& parentId) = 0;

    //Cleaning activities in the case of any error
    virtual void cleanDataOnFail() = 0;

    //The reader sends parent id of current <camera> tag to the child
    virtual void parentOfRootElemFound(const QString& parentId) = 0;

private:

    CameraSettingReader();
    bool parseCameraXml(const QDomElement& cameraXml);
    bool parseGroupXml(const QDomElement& groupXml, const QString parentId);
    bool parseElementXml(const QDomElement& elementXml, const QString parentId);
};

#endif //camera_settings_h_1726
