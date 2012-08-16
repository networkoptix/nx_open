#ifndef dw_resource_settings_h_1957
#define dw_resource_settings_h_1957

#include "../camera_settings/camera_settings.h"
#include "utils/network/simple_http_client.h"

//
// class DWCameraProxy
//

class DWCameraProxy
{
    CLSimpleHTTPClient m_httpClient;

public:

    DWCameraProxy(const QHostAddress& host, int port, unsigned int timeout, const QAuthenticator& auth);

    bool getFromCamera(const QString& name, QString& val);
    bool setToCamera(const QString& name, const QString& value, const QString& query);
};

//
// class DWCameraSetting
//

class DWCameraSetting: public CameraSetting
{
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
    DWCameraSettingReader(DWCameraSettings& settings, const QString& filepath);
    virtual ~DWCameraSettingReader();

protected:

    virtual bool isGroupEnabled(const QString& id, const QString& parentId, const QString& name);
    virtual bool isParamEnabled(const QString& id, const QString& parentId);
    virtual void paramFound(const CameraSetting& value, const QString& parentId);
    virtual void cleanDataOnFail();

private:

    DWCameraSettingReader();
};

#endif //dw_resource_settings_h_1957
