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
    QSet<QString> m_params;
    ParentOfRootElemFoundAware& m_obj;

public:
    CameraSettingsLister(const QString& id, ParentOfRootElemFoundAware& obj);
    virtual ~CameraSettingsLister();

    bool proceed(QSet<QString>& out);

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

    class WidgetAndParent
    {
        QTreeWidgetItem* m_widget;
        const QString m_parentId;


        WidgetAndParent();
        WidgetAndParent& operator= (const WidgetAndParent&);
        WidgetAndParent(const WidgetAndParent&);

    public:

        WidgetAndParent(QTreeWidgetItem* widget, const QString& parentId): m_widget(widget), m_parentId(parentId) {}
        ~WidgetAndParent() { delete m_widget; }

        QTreeWidgetItem* get() { return m_widget; }
        QTreeWidgetItem* take() { QTreeWidgetItem* tmp = m_widget; m_widget = 0; return tmp; }
        QString getParentId() { return m_parentId; }
    };

    typedef QHash<QString, WidgetAndParent*> WidgetsAndParentsById;

    typedef QHash<QString, int> LayoutIndById;

public:
    typedef QHash<QString, QTreeWidgetItem*> WidgetsById;

    CameraSettingsWidgetsCreator(const QString& id, ParentOfRootElemFoundAware& obj,
        QTreeWidget& rootWidget, QStackedLayout& rootLayout, WidgetsById& widgetsById, QObject* handler, QWidget* owner);

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
    void treeWidgetItemSelectionChanged();

private:

    CameraSettingsWidgetsCreator();

    void removeEmptyWidgetGroups();
    QTreeWidgetItem* findParentForParam(const QString& parentId);

    ParentOfRootElemFoundAware& m_obj;
    CameraSettings* m_settings;
    QTreeWidget& m_rootWidget;
    QStackedLayout& m_rootLayout;
    WidgetsById& m_widgetsById;
    QObject* m_handler;
    QWidget* m_owner;
    WidgetsAndParentsById m_emptyGroupsById;
    LayoutIndById m_layoutIndById;
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

class CameraSettingsTreeLister: public CameraSettingTreeReader<CameraSettingsLister, QSet<QString> >
{
    QSet<QString> m_params;

protected:
    virtual QSet<QString>& getCallback() override;

public:

    CameraSettingsTreeLister(const QString& id);

    CameraSettingsLister* createElement(const QString& id) override;

    QStringList proceed();
};

//
// class CameraSettingsWidgetsCreator
//

class CameraSettingsWidgetsTreeCreator: public CameraSettingTreeReader<CameraSettingsWidgetsCreator, CameraSettings>
{
    typedef CameraSettingsWidgetsCreator::WidgetsById WidgetsById;
    QTreeWidget& m_rootWidget;
    QStackedLayout& m_rootLayout;
    WidgetsById m_widgetsById;
    QObject* m_handler;
    QWidget* m_owner;
    CameraSettings* m_settings;

protected:
    virtual CameraSettings& getCallback() override;

public:

    CameraSettingsWidgetsTreeCreator(const QString& id, QTreeWidget& rootWidget, QStackedLayout& rootLayout, QObject* handler);

    CameraSettingsWidgetsCreator* createElement(const QString& id) override;

    void proceed(CameraSettings* settings);
};

#endif //camera_advanced_settings_xml_parser_h_1819
