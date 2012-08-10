#ifndef camera_advanced_settings_xml_parser_h_1819
#define camera_advanced_settings_xml_parser_h_1819

#include "plugins/resources/camera_settings/camera_settings.h"

typedef QHash<QString, CameraSetting> CameraSettings;
typedef QHash<QString, QWidget*> WidgetsById;

//
// class CameraSettingsLister
//

class CameraSettingsLister: public CameraSettingReader
{
    QStringList m_params;

public:
    CameraSettingsLister(const QString& filepath);
    virtual ~CameraSettingsLister();

    QStringList fetchParams();

protected:

    virtual bool isGroupEnabled(const QString& id, const QString& parentId, const QString& name);
    virtual bool isParamEnabled(const QString& id, const QString& parentId);
    virtual void paramFound(const CameraSetting& value, const QString& parentId);
    virtual void cleanDataOnFail();

private:

    CameraSettingsLister();
};

//
// class CameraSettingsWidgetsCreator
//

class CameraSettingsWidgetsCreator: public CameraSettingReader
{
    CameraSettings& m_settings;
    QTabWidget& m_rootWidget;
    WidgetsById m_widgetsById;

public:
    CameraSettingsWidgetsCreator(const QString& filepath, CameraSettings& settings, QTabWidget& rootWidget);
    virtual ~CameraSettingsWidgetsCreator();

protected:

    virtual bool isGroupEnabled(const QString& id, const QString& parentId, const QString& name);
    virtual bool isParamEnabled(const QString& id, const QString& parentId);
    virtual void paramFound(const CameraSetting& value, const QString& parentId);
    virtual void cleanDataOnFail();

private:

    CameraSettingsWidgetsCreator();
};

#endif //camera_advanced_settings_xml_parser_h_1819
