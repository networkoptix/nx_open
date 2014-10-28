#ifndef dw_resource_settings_h_1957
#define dw_resource_settings_h_1957

#include <QtCore/QStringList>

#include "../camera_settings/camera_settings.h"
#include "utils/network/simple_http_client.h"

class DWCameraSetting;

//
// class DWCameraProxy
//

class DWCameraProxy
{
    const QString m_host;
    int m_port;
    unsigned int m_timeout;
    const QAuthenticator m_auth;
    QHash<QString,QString> m_bufferedValues; //Camera settings values by query (query from camera_settings.xml)

public:

    DWCameraProxy(const QString& host, int port, unsigned int timeout, const QAuthenticator& auth);

    bool getFromCamera(DWCameraSetting& src);
    bool setToCamera(DWCameraSetting& src);

    bool getFromBuffer(DWCameraSetting& src);
    bool getFromCameraIntoBuffer();

private:

    bool getFromCameraImpl();
    bool getFromCameraImpl(const QByteArray& query);
    void fetchParamsFromHttpRespons(const QByteArray& body);
    bool setToCameraGet(DWCameraSetting& src, const QString& desiredVal);
    bool setToCameraPost(DWCameraSetting& src, const QString& desiredVal);
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
        const QString& method,
        const QString& description,
        const CameraSettingValue min = CameraSettingValue(),
        const CameraSettingValue max = CameraSettingValue(),
        const CameraSettingValue step = CameraSettingValue(),
        const CameraSettingValue current = CameraSettingValue());

    virtual ~DWCameraSetting() {};

    DWCameraSetting& operator=(const DWCameraSetting& rhs);

    bool getFromCamera(DWCameraProxy& proxy);
    bool getFromBuffer(DWCameraProxy& proxy);
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

    DWCameraSettingReader(DWCameraSettings& settings, const QString& cameraSettingId);
    virtual ~DWCameraSettingReader();

protected:

    virtual bool isGroupEnabled(const QString& id, const QString& parentId, const QString& name) override;
    virtual bool isParamEnabled(const QString& id, const QString& parentId) override;
    virtual void paramFound(const CameraSetting& value, const QString& parentId) override;
    virtual void cleanDataOnFail() override;
    virtual void parentOfRootElemFound(const QString& parentId) override;

private:

    DWCameraSettingReader();
};

#endif //dw_resource_settings_h_1957
