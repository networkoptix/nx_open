#ifndef dw_resource_settings_h_1957
#define dw_resource_settings_h_1957

#include "../camera_settings/camera_settings.h"
#include "utils/network/simple_http_client.h"

class DWCameraSetting;

//
// class DWCameraProxy
//

class DWCameraProxy
{
    CLSimpleHTTPClient m_httpClient;
    QHash<QString,QString> m_bufferedValues;

public:

    DWCameraProxy(const QHostAddress& host, int port, unsigned int timeout, const QAuthenticator& auth);

    bool getFromCamera(DWCameraSetting& src);
    bool setToCamera(DWCameraSetting& src);

private:

    bool getFromCameraImpl();
};

//
// class DWCameraSetting
//

class DWCameraSetting: public CameraSetting
{
    QStringList m_enumStrToInt;

public:

    DWCameraSetting(): CameraSetting() {};

    DWCameraSetting(const QString& id,
        const QString& name,
        WIDGET_TYPE type,
        const QString& query,
        const CameraSettingValue min = CameraSettingValue(),
        const CameraSettingValue max = CameraSettingValue(),
        const CameraSettingValue step = CameraSettingValue(),
        const CameraSettingValue current = CameraSettingValue());

    virtual ~DWCameraSetting() {};

    DWCameraSetting& DWCameraSetting::operator=(const DWCameraSetting& rhs);

    bool getFromCamera(DWCameraProxy& proxy);
    bool setToCamera(DWCameraProxy& proxy);
    QString getCurrentAsIntStr() const;
    QString getIntStrAsCurrent(const QString& numStr) const;

private:
    
    void initAdditionalValues();
};

typedef QHash<QString, DWCameraSetting> DWCameraSettings;

//
// class DWCameraSettingReader
//

class DWCameraSettingReader: public CameraSettingReader
{
    static const QString& IMAGING_GROUP_NAME;
    static const QString& MAINTENANCE_GROUP_NAME;

    DWCameraSettings& m_settings;

public:
    DWCameraSettingReader(DWCameraSettings& settings);
    virtual ~DWCameraSettingReader();

protected:

    virtual bool isGroupEnabled(const QString& id, const QString& parentId, const QString& name);
    virtual bool isParamEnabled(const QString& id, const QString& parentId);
    virtual void paramFound(const CameraSetting& value, const QString& parentId);
    virtual void cleanDataOnFail();
    virtual void parentOfRootElemFound(const QString& parentId);

private:

    DWCameraSettingReader();
};

#endif //dw_resource_settings_h_1957
