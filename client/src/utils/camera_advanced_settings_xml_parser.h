#ifndef camera_advanced_settings_xml_parser_h_1819
#define camera_advanced_settings_xml_parser_h_1819

#include "plugins/resources/camera_settings/camera_settings.h"

typedef QSharedPointer<CameraSetting> CameraSettingPtr;
typedef QHash<QString, CameraSettingPtr> CameraSettings;

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

class CameraSettingsWidgetsCreator: public QObject, public CameraSettingReader
{
    Q_OBJECT

    typedef QHash<QString, QWidget*> WidgetsById;

    CameraSettings* m_settings;
    QTreeWidget& m_rootWidget;
    QStackedLayout& m_rootLayout;
    WidgetsById m_widgetsById;
    QObject* m_handler;

public:
    CameraSettingsWidgetsCreator(const QString& filepath, QTreeWidget& rootWidget, QStackedLayout& rootLayout, QObject* handler);
    virtual ~CameraSettingsWidgetsCreator();

    bool recreateWidgets(CameraSettings* settings);

protected:

    virtual bool isGroupEnabled(const QString& id, const QString& parentId, const QString& name);
    virtual bool isParamEnabled(const QString& id, const QString& parentId);
    virtual void paramFound(const CameraSetting& value, const QString& parentId);
    virtual void cleanDataOnFail();

protected slots:
    void treeWidgetItemPressed(QTreeWidgetItem * item, int column);

private:

    CameraSettingsWidgetsCreator();
};

#endif //camera_advanced_settings_xml_parser_h_1819
