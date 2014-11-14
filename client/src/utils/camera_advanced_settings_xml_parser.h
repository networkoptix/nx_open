#ifndef camera_advanced_settings_xml_parser_h_1819
#define camera_advanced_settings_xml_parser_h_1819

#include <memory>

#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtCore/QSet>
#include <QtWidgets/QTreeWidgetItem>
#include <QtWidgets/QStackedLayout>
#include <QtWebKitWidgets/QtWebKitWidgets>

#include <core/resource/resource_fwd.h>
#include "plugins/resource/camera_settings/camera_settings.h"

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
    CameraSettingsLister(
        const QString& id,
        ParentOfRootElemFoundAware& obj,
        const QnResourcePtr& cameraRes = QnResourcePtr() );
    virtual ~CameraSettingsLister();

    //Set into 'out' all params from camera_settings.xml for desired camera
    bool proceed(QSet<QString>& out);

protected:

    virtual bool isGroupEnabled(const QString& id, const QString& parentId, const QString& name);
    virtual bool isParamEnabled(const QString& id, const QString& parentId);
    virtual void paramFound(const CameraSetting& value, const QString& parentId);
    virtual void cleanDataOnFail();
    virtual void parentOfRootElemFound(const QString& parentId) override;

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

    CameraSettingsWidgetsCreator(const QString& id, ParentOfRootElemFoundAware& obj, QTreeWidget* rootWidget, QStackedLayout* rootLayout,
        TreeWidgetItemsById& widgetsById, LayoutIndById& layoutIndById, SettingsWidgetsById& settingsWidgetsById, EmptyGroupsById& emptyGroupsById);

    virtual ~CameraSettingsWidgetsCreator();

    //Create or refresh advanced settings widgets according to info got from mediaserver ('settings' param)
    bool proceed(const CameraSettings& settings);

protected:

    virtual bool isGroupEnabled(const QString& id, const QString& parentId, const QString& name);
    virtual bool isParamEnabled(const QString& id, const QString& parentId);
    virtual void paramFound(const CameraSetting& value, const QString& parentId);
    virtual void cleanDataOnFail();
    virtual void parentOfRootElemFound(const QString& parentId) override;

protected slots:
    void treeWidgetItemSelectionChanged();

signals:
    void refreshAdvancedSettings();
    void advancedParamChanged(const CameraSetting &param);

private:
    void removeLayoutItems();
    QTreeWidgetItem* findParentForParam(const QString& parentId);
    bool isEnabledByOtherSettings(const QString& id, const QString& parentId);

    ParentOfRootElemFoundAware& m_obj;
    CameraSettings m_settings;
    QTreeWidget* m_rootWidget;
    QStackedLayout* m_rootLayout;
    TreeWidgetItemsById& m_treeWidgetsById;
    LayoutIndById& m_layoutIndById;
    SettingsWidgetsById& m_settingsWidgetsById;
    EmptyGroupsById& m_emptyGroupsById;
    QWidget* m_owner;
};

//
// class CameraSettingTreeReader
//

template <class T, class E>
class CameraSettingTreeReader: public ParentOfRootElemFoundAware
{
public:

    CameraSettingTreeReader(
        const QString& id,
        const QnResourcePtr& cameraRes = QnResourcePtr() );
    ~CameraSettingTreeReader();

    void setCamera( const QnResourcePtr& cameraRes )
    {
        m_cameraRes = cameraRes;
        for( T* obj: m_objList )
            obj->setCamera( cameraRes );
    }

    //Apply 'proceed' method of T class for all parents of current camera
    //(current camera is included also)
    bool proceed();

protected:
    QList<T*> m_objList;

    //Notification, that new parent was found
    virtual void parentOfRootElemFound(const QString& parentId) override;

    //Create T class to proceed new parent
    /*!
        \param dataSource If not NULL, data should be read fom here (It must be xml source).
            Otherwise, data is read from some internal source
    */
    virtual T* createElement(
        const QString& id,
        const QnResourcePtr& cameraRes ) = 0;

    //Method should return some object (additional info) required in proceed method of T class
    virtual E getAdditionalInfo() = 0;
    virtual void setAdditionalInfo(const E &value) = 0;

    //Cleaning activities
    void clean();

private:
    const QString m_initialId;
    QString m_currParentId;
    bool m_firstTime;
    QnResourcePtr m_cameraRes;
};

//
// class CameraSettingsTreeLister
//

class CameraSettingsTreeLister: public CameraSettingTreeReader<CameraSettingsLister, QSet<QString> >
{
    QSet<QString> m_params;

protected:
    virtual QSet<QString> getAdditionalInfo() override;
    virtual void setAdditionalInfo(const QSet<QString> &value) override;

public:

    CameraSettingsTreeLister(
        const QString& id,
        const QnResourcePtr& cameraRes = QnResourcePtr() );

    //!Implementation of CameraSettingTreeReader::createElement
    virtual CameraSettingsLister* createElement(
        const QString& id,
        const QnResourcePtr& cameraRes = QnResourcePtr() ) override;

    QStringList proceed();
};

//
// class CameraSettingsWidgetsCreator
//

class CameraSettingsWidgetsTreeCreator: public QObject, public CameraSettingTreeReader<CameraSettingsWidgetsCreator, CameraSettings>
{
    Q_OBJECT

    typedef CameraSettingsWidgetsCreator::TreeWidgetItemsById TreeWidgetItemsById;
    typedef CameraSettingsWidgetsCreator::LayoutIndById LayoutIndById;
    typedef CameraSettingsWidgetsCreator::SettingsWidgetsById SettingsWidgetsById;
    typedef CameraSettingsWidgetsCreator::EmptyGroupsById EmptyGroupsById;

public:
    CameraSettingsWidgetsTreeCreator(
        const QString& cameraId,
        const QString& id,
        QWebView* webView,
        QTreeWidget* rootWidget,
        QStackedLayout* rootLayout
        );
    ~CameraSettingsWidgetsTreeCreator();

    //!Implementation of CameraSettingTreeReader::createElement
    virtual CameraSettingsWidgetsCreator* createElement(
        const QString& id,
        const QnResourcePtr& cameraRes = QnResourcePtr() ) override;

    void proceed(const CameraSettings &settings);

    QString getId() const;
    QString getCameraId() const;
    QTreeWidget* getRootWidget();
    QStackedLayout* getRootLayout();
    QWebView* getWebView();

signals:
    void advancedParamChanged(const CameraSetting &param);
    void refreshAdvancedSettings();

protected:
    virtual CameraSettings getAdditionalInfo() override;
    virtual void setAdditionalInfo(const CameraSettings &value) override;

private:
    void removeEmptyWidgetGroups();

private:
    QWebView* m_webView;
    QTreeWidget* m_rootWidget;
    QStackedLayout* m_rootLayout;
    TreeWidgetItemsById m_treeWidgetsById;
    LayoutIndById m_layoutIndById;
    SettingsWidgetsById m_settingsWidgetsById;
    EmptyGroupsById m_emptyGroupsById;
    CameraSettings m_settings;
    const QString m_id;
    const QString m_cameraId;
};

#endif //camera_advanced_settings_xml_parser_h_1819
