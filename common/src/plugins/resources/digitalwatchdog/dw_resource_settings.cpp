#include "dw_resource_settings.h"

//
// class DWCameraProxy
//

DWCameraProxy::DWCameraProxy(const QHostAddress& host, int port, unsigned int timeout, const QAuthenticator& auth):
    m_httpClient(host, port, timeout, auth)
{
    CLHttpStatus status = m_httpClient.doGET(QByteArray("/cgi-bin/getconfig.cgi?action=color"));
    if (status == CL_HTTP_SUCCESS) 
    {
        QByteArray body;
        m_httpClient.readAll(body);
        QList<QByteArray> lines = body.split(',');
        for (int i = 0; i < lines.size(); ++i) 
        {
            QString str = QString::fromLatin1(lines[i]);
            /*if (lines[i].toLower().contains("onvif_stream_number")) 
            {
                QList<QByteArray> params = lines[i].split(':');
                if (params.size() >= 2) 
                {
                    int streams = params[1].trimmed().toInt();

                    return streams >= 2;
                }
            }*/
        }
    }

    //ToDo: log warning
}

bool DWCameraProxy::getFromCamera(const QString& name, QString& val)
{
    //ToDo: implement
    return false;
}

bool DWCameraProxy::setToCamera(const QString& name, const QString& value, const QString& query)
{
    //ToDo: implement
    return false;
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

DWCameraSettingReader::DWCameraSettingReader(DWCameraSettings& settings, const QString& filepath):
    CameraSettingReader(filepath, QString::fromLatin1("DIGITALWATCHDOG")),
    m_settings(settings)
{

}

DWCameraSettingReader::~DWCameraSettingReader()
{

}

bool DWCameraSettingReader::isGroupEnabled(const QString& id, const QString& /*parentId*/, const QString& /*name*/)
{
    //ToDo: check for the duplicated id
    m_settings.insert(id, DWCameraSetting());
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
