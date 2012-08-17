#ifndef camera_advanced_settings_xml_parser_h_1819
#define camera_advanced_settings_xml_parser_h_1819

#include "plugins/resources/camera_settings/camera_settings.h"

typedef QSharedPointer<CameraSetting> CameraSettingPtr;
typedef QHash<QString, CameraSettingPtr> CameraSettings;

//
// interface ParentOfRootElemFoundAware
//

class ParentOfRootElemFoundAware
{
public:
    virtual ~ParentOfRootElemFoundAware() {}
    virtual void parentOfRootElemFound(const QString& parentId) = 0;
};



//
// class CameraSettingsLister
//

class CameraSettingsLister: public CameraSettingReader
{
    QStringList m_params;
    ParentOfRootElemFoundAware& m_obj;

public:
    CameraSettingsLister(const QString& filepath, const QString& id, ParentOfRootElemFoundAware& obj);
    virtual ~CameraSettingsLister();

    bool proceed(QStringList& out);

protected:

    virtual bool isGroupEnabled(const QString& id, const QString& parentId, const QString& name);
    virtual bool isParamEnabled(const QString& id, const QString& parentId);
    virtual void paramFound(const CameraSetting& value, const QString& parentId);
    virtual void cleanDataOnFail();
    virtual void parentOfRootElemFound(const QString& parentId);

private:

    CameraSettingsLister();
};

//
// class CameraSettingsWidgetsCreator
//

class CameraSettingsWidgetsCreator: public QObject, public CameraSettingReader
{
    Q_OBJECT

    typedef QHash<QString, QTreeWidgetItem*> WidgetsById;

    ParentOfRootElemFoundAware& m_obj;
    CameraSettings* m_settings;
    QTreeWidget& m_rootWidget;
    QStackedLayout& m_rootLayout;
    WidgetsById m_widgetsById;
    QObject* m_handler;
    QWidget* m_owner;

public:
    CameraSettingsWidgetsCreator(const QString& filepath, const QString& id, ParentOfRootElemFoundAware& obj,
        QTreeWidget& rootWidget, QStackedLayout& rootLayout, QObject* handler);

    virtual ~CameraSettingsWidgetsCreator();

    bool proceed(CameraSettings& settings);

protected:

    virtual bool isGroupEnabled(const QString& id, const QString& parentId, const QString& name);
    virtual bool isParamEnabled(const QString& id, const QString& parentId);
    virtual void paramFound(const CameraSetting& value, const QString& parentId);
    virtual void cleanDataOnFail();
    virtual void parentOfRootElemFound(const QString& parentId);

protected slots:
    void treeWidgetItemPressed(QTreeWidgetItem * item, int column);

private:

    CameraSettingsWidgetsCreator();
};

//
// class CameraSettingTreeReader
//

template <class T, class E>
class CameraSettingTreeReader: public ParentOfRootElemFoundAware
{
    const QString m_initialId;
    QString m_currParentId;
    bool m_firstTime;

protected:

    QList<T*> m_objList;

    virtual void parentOfRootElemFound(const QString& parentId);
    virtual T* createElement(const QString& id) = 0;
    virtual E& getCallback() = 0;
    void clean();

public:

    CameraSettingTreeReader(const QString& id);
    ~CameraSettingTreeReader();

    bool proceed();
};

//
// class CameraSettingsTreeLister
//

class CameraSettingsTreeLister: public CameraSettingTreeReader<CameraSettingsLister, QStringList>
{
    const QString& m_filepath;
    QStringList m_params;

protected:
    virtual QStringList& getCallback() override;

public:

    CameraSettingsTreeLister(const QString& filepath, const QString& id);

    CameraSettingsLister* createElement(const QString& id) override;

    QStringList proceed();
};

//
// class CameraSettingsWidgetsCreator
//

class CameraSettingsWidgetsTreeCreator: public CameraSettingTreeReader<CameraSettingsWidgetsCreator, CameraSettings>
{
    const QString& m_filepath;
    QTreeWidget& m_rootWidget;
    QStackedLayout& m_rootLayout;
    QObject* m_handler;
    CameraSettings* m_settings;

protected:
    virtual CameraSettings& getCallback() override;

public:

    CameraSettingsWidgetsTreeCreator(const QString& filepath, const QString& id, QTreeWidget& rootWidget, QStackedLayout& rootLayout, QObject* handler);

    CameraSettingsWidgetsCreator* createElement(const QString& id) override;

    void proceed(CameraSettings* settings);
};

#endif //camera_advanced_settings_xml_parser_h_1819
