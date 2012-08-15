#include "dw_resource_settings.h"

//
// class DWCameraProxy
//

DWCameraProxy::DWCameraProxy(const QHostAddress& host, int port, unsigned int timeout, const QAuthenticator& auth):
    m_httpClient(host, port, timeout, auth)
{

}

bool DWCameraProxy::getFromCamera(const QString& name, QString& val)
{
    //ToDo: implement
    return true;
}

bool DWCameraProxy::setToCamera(const QString& name, const QString& value, const QString& query)
{
    //ToDo: implement
    return true;
}

//
// class DWCameraSetting: public CameraSetting
// 

DWCameraSetting::DWCameraSetting(const QString& id, const QString& name, WIDGET_TYPE type, const QString& query, const CameraSettingValue min,
        const CameraSettingValue max, const CameraSettingValue step, const CameraSettingValue current) :
    CameraSetting(id, name, type, query, min, max, step, current)
{

}

DWCameraSetting& DWCameraSetting::operator=(const DWCameraSetting& rhs)
{
    CameraSetting::operator=(rhs);

    return *this;
}

bool DWCameraSetting::getFromCamera(DWCameraProxy& proxy)
{
    QString tmp;
    bool res = proxy.getFromCamera(getId(), tmp);
    if (res) {
        setCurrent(tmp);
    }
    return res;
}

bool DWCameraSetting::setToCamera(DWCameraProxy& proxy)
{
    return proxy.setToCamera(getId(), static_cast<QString>(getCurrent()), getQuery());
}

//
// class DWCameraSettingReader
//

const QString& DWCameraSettingReader::IMAGING_GROUP_NAME = *(new QString(QLatin1String("%%Imaging")));
const QString& DWCameraSettingReader::MAINTENANCE_GROUP_NAME = *(new QString(QLatin1String("%%Maintenance")));

DWCameraSettingReader::DWCameraSettingReader(CameraSettings& settings, const QString& filepath):
    CameraSettingReader(filepath, QString::fromLatin1("DIGITALWATCHDOG")),
    m_settings(settings)
{

}

DWCameraSettingReader::~DWCameraSettingReader()
{

}

bool DWCameraSettingReader::isGroupEnabled(const QString& id, const QString& /*parentId*/, const QString& /*name*/)
{
    if (m_settings.isEmpty() && id == IMAGING_GROUP_NAME)
    {
        //Group is enabled by default, to disable we should make a record, which equals empty object
        m_settings.insert(id, DWCameraSetting());

        return false;
    }

    return true;
}

bool DWCameraSettingReader::isParamEnabled(const QString& /*id*/, const QString& /*parentId*/)
{
    return true;
}

void DWCameraSettingReader::paramFound(const CameraSetting& value, const QString& /*parentId*/)
{
    m_settings.insert(value.getId(), DWCameraSetting(
        value.getId(), value.getName(), value.getType(), value.getQuery(), value.getMin(), value.getMax(), value.getStep()));
}

void DWCameraSettingReader::cleanDataOnFail()
{
    m_settings.clear();
}
