#include "camera_advanced_settings_xml_parser.h"

const QString CASXP_IMAGING_GROUP_NAME(QLatin1String("%%Imaging"));
const QString CASXP_MAINTENANCE_GROUP_NAME(QLatin1String("%%Maintenance"));

//
// class CameraSettingsLister
//

CameraSettingsLister::CameraSettingsLister(const QString& filepath):
    CameraSettingReader(filepath, QString::fromLatin1("ONVIF"))
{

}

CameraSettingsLister::~CameraSettingsLister()
{

}

QStringList CameraSettingsLister::fetchParams()
{
    read();
    proceed();

    return m_params;
}

bool CameraSettingsLister::isGroupEnabled(const QString& id)
{
    m_params.push_back(id);

    return true;
}

bool CameraSettingsLister::isParamEnabled(const QString& id, const QString& /*parentId*/)
{
    m_params.push_back(id);

    //We don't need creation of the object, so returning false
    return false;
}

void CameraSettingsLister::paramFound(const CameraSetting& /*value*/, const QString& /*parentId*/)
{
    //This code is unreachable
    Q_ASSERT(false);
}

void CameraSettingsLister::cleanDataOnFail()
{
    m_params.clear();
}

//
// class CameraSettingsWidgetsCreator
//

CameraSettingsWidgetsCreator::CameraSettingsWidgetsCreator(const QString& filepath):
CameraSettingReader(filepath, QString::fromLatin1("ONVIF"))
{

}

CameraSettingsWidgetsCreator::~CameraSettingsWidgetsCreator()
{

}

QStringList CameraSettingsWidgetsCreator::fetchParams()
{
    read();
    proceed();

    return m_params;
}

bool CameraSettingsWidgetsCreator::isGroupEnabled(const QString& id)
{
    m_params.push_back(id);

    return true;
}

bool CameraSettingsWidgetsCreator::isParamEnabled(const QString& id, const QString& /*parentId*/)
{
    m_params.push_back(id);

    //We don't need creation of the object, so returning false
    return false;
}

void CameraSettingsWidgetsCreator::paramFound(const CameraSetting& /*value*/, const QString& /*parentId*/)
{
    //This code is unreachable
    Q_ASSERT(false);
}

void CameraSettingsWidgetsCreator::cleanDataOnFail()
{
    m_params.clear();
}
