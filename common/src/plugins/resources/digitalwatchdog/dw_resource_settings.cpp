#include "dw_resource_settings.h"

//
// class DWCameraProxy
//

QRegExp DW_RES_SETTINGS_FILTER(QString::fromLatin1("[{},]"));


DWCameraProxy::DWCameraProxy(const QHostAddress& host, int port, unsigned int timeout, const QAuthenticator& auth):
    m_host(host),
    m_port(port),
    m_timeout(timeout),
    m_auth(auth)
{
    getFromCameraImpl();
}

bool DWCameraProxy::getFromCameraImpl()
{
    CLSimpleHTTPClient httpClient(m_host, m_port, m_timeout, m_auth);

    CLHttpStatus status = httpClient.doGET(QByteArray("cgi-bin/getconfig.cgi?action=color"));
    if (status == CL_HTTP_SUCCESS) 
    {
        m_bufferedValues.clear();

        QByteArray body;
        httpClient.readAll(body);
        QList<QByteArray> lines = body.split(',');
        for (int i = 0; i < lines.size(); ++i) 
        {
            QString str = QString::fromLatin1(lines[i]);
            str.replace(DW_RES_SETTINGS_FILTER, QString());
            QStringList pairStrs = str.split(QString::fromLatin1(":"));
            if (pairStrs.size() == 2) {
                m_bufferedValues.insert(pairStrs[0].trimmed(), pairStrs[1].trimmed());
            }
        }

        return true;
    }

    //ToDo: log warning
    return false;
}

bool DWCameraProxy::getFromCamera(DWCameraSetting& src)
{
    if (src.getQuery().startsWith(QLatin1String("/"))) {
        return true;
    }

    if (!getFromCameraImpl()) {
        return false;
    }

    QHash<QString,QString>::ConstIterator it = m_bufferedValues.find(src.getQuery());
    if (it == m_bufferedValues.end()) {
        return false;
    }

    src.setCurrent(src.getIntStrAsCurrent(it.value()));
    return true;
}

bool DWCameraProxy::setToCamera(DWCameraSetting& src)
{
    QString desiredVal = src.getCurrentAsIntStr();

    CLSimpleHTTPClient httpClient(m_host, m_port, m_timeout, m_auth);
    QString paramQuery = src.getQuery();
    QByteArray query = paramQuery.startsWith(QLatin1String("/"))? paramQuery.toLatin1() : 
        (QString::fromLatin1("cgi-bin/camerasetup.cgi?") + paramQuery + QString::fromLatin1("=") + desiredVal).toLatin1();

    bool res = httpClient.doGET(query) == CL_HTTP_SUCCESS;
    res = getFromCameraImpl() && res;
    res = getFromCamera(src) && res;    

    return res && desiredVal == src.getCurrentAsIntStr();
}

bool DWCameraProxy::getFromBuffer(DWCameraSetting& src)
{
    if (src.getQuery().startsWith(QLatin1String("/"))) {
        return true;
    }

    QHash<QString,QString>::ConstIterator it = m_bufferedValues.find(src.getQuery());
    if (it == m_bufferedValues.end()) {
        return false;
    }

    src.setCurrent(src.getIntStrAsCurrent(it.value()));
    return true;
}

bool DWCameraProxy::getFromCameraIntoBuffer()
{
    return getFromCameraImpl();
}

//
// class DWCameraSetting: public CameraSetting
// 

DWCameraSetting::DWCameraSetting(const QString& id, const QString& name, WIDGET_TYPE type, const QString& query, const QString& description,
        const CameraSettingValue min, const CameraSettingValue max, const CameraSettingValue step, const CameraSettingValue current) :
    CameraSetting(id, name, type, query, description, min, max, step, current)
{
    initAdditionalValues();
}

DWCameraSetting& DWCameraSetting::operator=(const DWCameraSetting& rhs)
{
    CameraSetting::operator=(rhs);

    m_enumStrToInt = rhs.m_enumStrToInt;

    return *this;
}

bool DWCameraSetting::getFromCamera(DWCameraProxy& proxy)
{
    return proxy.getFromCamera(*this);
}

bool DWCameraSetting::getFromBuffer(DWCameraProxy& proxy)
{
    return proxy.getFromBuffer(*this);
}

bool DWCameraSetting::setToCamera(DWCameraProxy& proxy)
{
    return proxy.setToCamera(*this);
}

void DWCameraSetting::initAdditionalValues()
{
    if (getType() != CameraSetting::Enumeration && getType() != CameraSetting::OnOff ||
        !m_enumStrToInt.isEmpty())
    {
        return;
    }

    if (getType() == CameraSetting::Enumeration)
    {
        m_enumStrToInt = static_cast<QString>(getMin()).split(QLatin1String(","));
        return;
    }

    if (getType() == CameraSetting::OnOff)
    {
        m_enumStrToInt.push_back(getMin());
        m_enumStrToInt.push_back(getMax());
        return;
    }
}

QString DWCameraSetting::getCurrentAsIntStr() const
{
    if (getType() != CameraSetting::Enumeration && getType() != CameraSetting::OnOff)
    {
        return static_cast<QString>(getCurrent());
    }

    QString curr = static_cast<QString>(getCurrent());
    for (int i = 0; i < m_enumStrToInt.size(); ++i) {
        if (curr == m_enumStrToInt.at(i)) {
            QString numStr;
            numStr.setNum(i);
            return numStr;
        }
    }

    //Log warning
    return QString();
}

QString DWCameraSetting::getIntStrAsCurrent(const QString& numStr) const
{
    if (getType() != CameraSetting::Enumeration && getType() != CameraSetting::OnOff)
    {
        return numStr;
    }

    int i = numStr.toInt();
    if (i < 0 || i >= m_enumStrToInt.size()) {
        //ToDo: Log warning, currently let's be assert
        Q_ASSERT(false);
        return QString();
    }

    return m_enumStrToInt.at(i);
}

//
// class DWCameraSettingReader
//

const QString& DWCameraSettingReader::IMAGING_GROUP_NAME = *(new QString(QLatin1String("%%Imaging")));
const QString& DWCameraSettingReader::MAINTENANCE_GROUP_NAME = *(new QString(QLatin1String("%%Maintenance")));

DWCameraSettingReader::DWCameraSettingReader(DWCameraSettings& settings, const QString& cameraSettingId):
    CameraSettingReader(cameraSettingId),
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
    m_settings.insert(value.getId(), DWCameraSetting(value.getId(), value.getName(), value.getType(), value.getQuery(),
        value.getDescription(), value.getMin(), value.getMax(), value.getStep()));
}

void DWCameraSettingReader::cleanDataOnFail()
{
    m_settings.clear();
}

void DWCameraSettingReader::parentOfRootElemFound(const QString& /*parentId*/)
{

}
