#ifndef camera_advanced_settings_xml_parser_h_1819
#define camera_advanced_settings_xml_parser_h_1819

#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtCore/QSet>
#include <QtWidgets/QTreeWidgetItem>
#include <QtWidgets/QStackedLayout>
#ifdef QT_WEBKITWIDGETS_LIB
#include <QtWebKitWidgets/QtWebKitWidgets>
#endif

#include "plugins/resources/camera_settings/camera_settings.h"

typedef QSharedPointer<CameraSetting> CameraSettingPtr;
typedef QHash<QString, CameraSettingPtr> CameraSettings;
class QnAbstractSettingsWidget;

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

    //Set into 'out' all params from camera_settings.xml for desired camera
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

public:
    typedef QHash<QString, QTreeWidgetItem*> TreeWidgetItemsById;
    typedef QHash<QString, int> LayoutIndById;
    typedef QHash<QString, QnAbstractSettingsWidget*> SettingsWidgetsById;
    typedef QHash<QString, WidgetAndParent*> EmptyGroupsById;

    CameraSettingsWidgetsCreator(const QString& id, ParentOfRootElemFoundAware& obj, QTreeWidget& rootWidget, QStackedLayout& rootLayout,
        TreeWidgetItemsById& widgetsById, LayoutIndById& layoutIndById, SettingsWidgetsById& settingsWidgetsById, EmptyGroupsById& emptyGroupsById, QObject* handler);

    virtual ~CameraSettingsWidgetsCreator();

    //Create or refresh advanced settings widgets according to info got from mediaserver ('settings' param)
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

signals:
    void refreshAdvancedSettings();

private:

    CameraSettingsWidgetsCreator();

    void removeLayoutItems();
    QTreeWidgetItem* findParentForParam(const QString& parentId);
    bool isEnabledByOtherSettings(const QString& id, const QString& parentId);

    ParentOfRootElemFoundAware& m_obj;
    CameraSettings* m_settings;
    QTreeWidget& m_rootWidget;
    QStackedLayout& m_rootLayout;
    TreeWidgetItemsById& m_treeWidgetsById;
    LayoutIndById& m_layoutIndById;
    SettingsWidgetsById& m_settingsWidgetsById;
    EmptyGroupsById& m_emptyGroupsById;
    QObject* m_handler;
    QWidget* m_owner;
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

    //Notification, that new parent was found
    virtual void parentOfRootElemFound(const QString& parentId);

    //Create T class to proceed new parent
    virtual T* createElement(const QString& id) = 0;

    //Method should return some object (additional info) required in proceed method of T class
    virtual E& getAdditionalInfo() = 0;

    //Cleaning activities
    void clean();

public:

    CameraSettingTreeReader(const QString& id);
    ~CameraSettingTreeReader();

    //Apply 'proceed' method of T class for all parents of current camera
    //(current camera is included also)
    bool proceed();
};

//
// class CameraSettingsTreeLister
//

class CameraSettingsTreeLister: public CameraSettingTreeReader<CameraSettingsLister, QSet<QString> >
{
    QSet<QString> m_params;

protected:
    virtual QSet<QString>& getAdditionalInfo() override;

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
    typedef CameraSettingsWidgetsCreator::TreeWidgetItemsById TreeWidgetItemsById;
    typedef CameraSettingsWidgetsCreator::LayoutIndById LayoutIndById;
    typedef CameraSettingsWidgetsCreator::SettingsWidgetsById SettingsWidgetsById;
    typedef CameraSettingsWidgetsCreator::EmptyGroupsById EmptyGroupsById;

#ifdef QT_WEBKITWIDGETS_LIB
    QWebView* m_webView;
#endif
    QTreeWidget& m_rootWidget;
    QStackedLayout& m_rootLayout;
    TreeWidgetItemsById m_treeWidgetsById;
    LayoutIndById m_layoutIndById;
    SettingsWidgetsById m_settingsWidgetsById;
    EmptyGroupsById m_emptyGroupsById;
    QObject* m_handler;
    CameraSettings* m_settings;
    QString m_id;
    QString m_cameraId;

protected:
    virtual CameraSettings& getAdditionalInfo() override;

private:
    void removeEmptyWidgetGroups();

public:

    CameraSettingsWidgetsTreeCreator(
        const QString& cameraId,
        const QString& id,
        QTreeWidget& rootWidget,
        QStackedLayout& rootLayout,
#ifdef QT_WEBKITWIDGETS_LIB
        QWebView* webView,
#endif
        QObject* handler);
    ~CameraSettingsWidgetsTreeCreator();

    CameraSettingsWidgetsCreator* createElement(const QString& id) override;

    void proceed(CameraSettings* settings);

    QString getId() const;
    QString getCameraId() const;
    QTreeWidget* getRootWidget();
    QStackedLayout* getRootLayout();
#ifdef QT_WEBKITWIDGETS_LIB
    QWebView* getWebView();
#endif
};

#endif //camera_advanced_settings_xml_parser_h_1819
