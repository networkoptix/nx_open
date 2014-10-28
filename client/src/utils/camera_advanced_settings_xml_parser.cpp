
#include "camera_advanced_settings_xml_parser.h"

#include <QtCore/QDebug>

#include "ui/widgets/properties/camera_advanced_settings_widget.h"


const QString CASXP_IMAGING_GROUP_NAME(QLatin1String("%%Imaging"));
const QString CASXP_MAINTENANCE_GROUP_NAME(QLatin1String("%%Maintenance"));

//
// class CameraSettingsLister
//

CameraSettingsLister::CameraSettingsLister(
    const QString& id,
    ParentOfRootElemFoundAware& obj,
    const QnResourcePtr& cameraRes )
:
    CameraSettingReader(id, cameraRes),
    m_obj(obj)
{

}

CameraSettingsLister::~CameraSettingsLister()
{

}

bool CameraSettingsLister::proceed(QSet<QString>& out)
{
    bool res = read() && CameraSettingReader::proceed();

    if (res) {
        out.unite(m_params);
        return true;
    }

    return false;
}

bool CameraSettingsLister::isGroupEnabled(const QString& id, const QString& /*parentId*/, const QString& /*name*/)
{
    m_params.insert(id);

    return true;
}

bool CameraSettingsLister::isParamEnabled(const QString& id, const QString& /*parentId*/)
{
    m_params.insert(id);

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

void CameraSettingsLister::parentOfRootElemFound(const QString& parentId)
{
    m_obj.parentOfRootElemFound(parentId);
}

//
// class CameraSettingsWidgetsCreator
//

CameraSettingsWidgetsCreator::CameraSettingsWidgetsCreator(
    const QString& id,
    ParentOfRootElemFoundAware& obj,
    QTreeWidget& rootWidget,
    QStackedLayout& rootLayout,
    TreeWidgetItemsById& widgetsById,
    LayoutIndById& layoutIndById,
    SettingsWidgetsById& settingsWidgetsById,
    EmptyGroupsById& emptyGroupsById )
:
    CameraSettingReader(id),
    m_obj(obj),
    m_settings(0),
    m_rootWidget(rootWidget),
    m_rootLayout(rootLayout),
    m_treeWidgetsById(widgetsById),
    m_layoutIndById(layoutIndById),
    m_settingsWidgetsById(settingsWidgetsById),
    m_emptyGroupsById(emptyGroupsById),
    m_owner(0)
{
    connect( &m_rootWidget, &QTreeWidget::itemSelectionChanged, this, &CameraSettingsWidgetsCreator::treeWidgetItemSelectionChanged );
}

CameraSettingsWidgetsCreator::~CameraSettingsWidgetsCreator()
{
    removeLayoutItems();
}

void CameraSettingsWidgetsCreator::removeLayoutItems()
{
    if (m_owner)
    {
        QObjectList children = m_owner->children();
        for (int i = 0; i < children.count(); ++i)
        {
            m_rootLayout.removeWidget(static_cast<QnSettingsScrollArea*>(children[i]));
        }
        delete m_owner;
        m_owner = 0;
    }
}

bool CameraSettingsWidgetsCreator::proceed(CameraSettings& settings)
{
    m_settings = &settings;

    return read() && CameraSettingReader::proceed();
}

bool CameraSettingsWidgetsCreator::isGroupEnabled(const QString& id, const QString& parentId, const QString& name)
{
    if (m_treeWidgetsById.find(id) != m_treeWidgetsById.end()) {
        return true;
    }

    if (m_settings->find(id) != m_settings->end()) {
        //By default, group is enabled. It can be disabled by mediaserver
        //Disabled - means empty value. Group can have only empty value, so 
        //if we found it - it is disabled
        return false;
    }

    if (m_emptyGroupsById.find(id) == m_emptyGroupsById.end())
    {
        QTreeWidgetItem* item = new QTreeWidgetItem((QTreeWidget*)0, QStringList(name));
        item->setData(0, Qt::UserRole, 0);
        m_emptyGroupsById.insert(id, new WidgetAndParent(item, parentId));
    }

    return true;
}

bool CameraSettingsWidgetsCreator::isEnabledByOtherSettings(const QString& id, const QString& /*parentId*/)
{
    //ToDo: remove hardcode
    if (!getCameraId().startsWith(lit("DIGITALWATCHDOG"))) {
        return true;
    }

    if (id == lit("%%Imaging%%White Balance%%Kelvin") ||
        id == lit("%%Imaging%%White Balance%%Red") ||
        id == lit("%%Imaging%%White Balance%%Blue") )
    {
        CameraSettings::Iterator settingIt = m_settings->find(lit("%%Imaging%%White Balance%%Mode"));
        QString val = settingIt != m_settings->end() ? static_cast<QString>(settingIt.value()->getCurrent()) : QString();
        if (val == lit("MANUAL")) {
            return true;
        }
        return false;
    }

    if (id == lit("%%Imaging%%Exposure%%Shutter Speed") )
    {
        CameraSettings::Iterator settingIt = m_settings->find(lit("%%Imaging%%Exposure%%Shutter Mode"));
        QString val = settingIt != m_settings->end() ? static_cast<QString>(settingIt.value()->getCurrent()) : QString();
        if (val == lit("MANUAL")) {
            return true;
        }
        return false;
    }

    if (id == lit("%%Imaging%%Day & Night%%Switching from Color to B/W") ||
        id == lit("%%Imaging%%Day & Night%%Switching from B/W to Color") )
    {
        CameraSettings::Iterator settingIt = m_settings->find(lit("%%Imaging%%Day & Night%%Mode"));
        QString val = settingIt != m_settings->end() ? static_cast<QString>(settingIt.value()->getCurrent()) : QString();
        if (val == lit("AUTO")) {
            return true;
        }
        return false;
    }

    return true;
}

bool CameraSettingsWidgetsCreator::isParamEnabled(const QString& id, const QString& parentId)
{
    bool enabled = isEnabledByOtherSettings(id, parentId);

    SettingsWidgetsById::Iterator swbiIt = m_settingsWidgetsById.find(id);
    if (swbiIt != m_settingsWidgetsById.end())
    {
        if (enabled) {
            swbiIt.value()->refresh();
        } else {
            delete swbiIt.value();
            m_settingsWidgetsById.erase(swbiIt);
        }

        //Already created, so we don't need creation, so returning false
        return false;
    }

    if (!enabled) {
        return false;
    }

    CameraSettings::ConstIterator it = m_settings->find(id);
    if (it == m_settings->end()) {
        //It means enabled, but have no value
        return true;
    }

    return isEnabled(*(it.value()));
}

void CameraSettingsWidgetsCreator::paramFound(const CameraSetting& value, const QString& parentId)
{
    CameraSettings::Iterator currIt = m_settings->find(value.getId());
    if (currIt == m_settings->end() && value.getType() != CameraSetting::Button) {
        //ToDo: will be nice to prevent mediaserver send disabled params;
        return;
    }

    //TODO #ak this is bullshit!
        //parameter description must be taken from single place: xml or mediaserver reply (m_settings), not both!
        //xml is preferred so that camera_settings.xml is parsed on client side only. 
        //In this case mediaserver reply must contain only param values, not description
    if( currIt != m_settings->end() )
        currIt.value()->setIsReadOnly( value.isReadOnly() );

    QnSettingsScrollArea* rootWidget = 0;
    LayoutIndById::ConstIterator lIndIt = m_layoutIndById.find(parentId);
    if (lIndIt != m_layoutIndById.end()) {
        int ind = lIndIt.value();
        rootWidget = dynamic_cast<QnSettingsScrollArea*>(m_rootLayout.widget(ind));
        Q_ASSERT(rootWidget != 0);
    } else {
        QTreeWidgetItem* parentItem = findParentForParam(parentId);
        rootWidget = new QnSettingsScrollArea(m_owner);
        int ind = m_rootLayout.addWidget(rootWidget);
        m_layoutIndById.insert(parentId, ind);
        parentItem->setData(0, Qt::UserRole, ind);
    }

    //Description is not transported through http to avoid huge requests, so
    //setting it from camera_settings.xml file.
    if (currIt != m_settings->end())
        currIt.value()->setDescription(value.getDescription());

    QnAbstractSettingsWidget* tabWidget = 0;
    switch(value.getType())
    {
        case CameraSetting::OnOff:
            tabWidget = new QnSettingsOnOffWidget(*(currIt.value()), rootWidget);
            break;

        case CameraSetting::MinMaxStep:
            tabWidget = new QnSettingsMinMaxStepWidget(*(currIt.value()), rootWidget);
            break;

        case CameraSetting::Enumeration:
            tabWidget = new QnSettingsEnumerationWidget(*(currIt.value()), rootWidget);
            break;

        case CameraSetting::Button:
            tabWidget = new QnSettingsButtonWidget(value, rootWidget);
            break;

        case CameraSetting::TextField:
            tabWidget = new QnSettingsTextFieldWidget(*(currIt.value()), rootWidget);
            break;

        case CameraSetting::ControlButtonsPair:
            tabWidget = new QnSettingsControlButtonsPairWidget(*(currIt.value()), rootWidget);
            break;

        default:
            //Unknown widget type!
            Q_ASSERT(false);
    }

    connect(tabWidget, &QnAbstractSettingsWidget::advancedParamChanged, this, &CameraSettingsWidgetsCreator::advancedParamChanged);
    m_settingsWidgetsById.insert(value.getId(), tabWidget);
}

QTreeWidgetItem* CameraSettingsWidgetsCreator::findParentForParam(const QString& parentId)
{
    TreeWidgetItemsById::ConstIterator it = m_treeWidgetsById.find(parentId);
    if (it != m_treeWidgetsById.end()) {
        return it.value();
    }

    QList<QPair<QString, WidgetAndParent*> > parentsHierarchy;

    QString grandParentId = parentId;
    while (true)
    {
        EmptyGroupsById::Iterator eGroupsIt = m_emptyGroupsById.find(grandParentId);
        if (eGroupsIt == m_emptyGroupsById.end()) {
            break;
        }

        parentsHierarchy.push_front(QPair<QString, WidgetAndParent*>(grandParentId, eGroupsIt.value()));
        grandParentId = eGroupsIt.value()->getParentId();
        m_emptyGroupsById.erase(eGroupsIt);
    }


    if (grandParentId.isEmpty())
    {
        if (parentsHierarchy.isEmpty()) {
            //Param can't be attached directly to m_rootWidget
            Q_ASSERT(false);
        }

        QPair<QString, WidgetAndParent*> data = parentsHierarchy.takeFirst();
        m_rootWidget.addTopLevelItem(data.second->get());
        it = m_treeWidgetsById.insert(data.first, data.second->take());

        delete data.second;
    }
    else
    {
        it = m_treeWidgetsById.find(grandParentId);
    }

    if (it == m_treeWidgetsById.end()) {
        //Grand parent must exist!
        Q_ASSERT(false);
    }

    while (!parentsHierarchy.isEmpty())
    {
        QPair<QString, WidgetAndParent*> data = parentsHierarchy.takeFirst();
        it.value()->addChild(data.second->get());
        it = m_treeWidgetsById.insert(data.first, data.second->take());

        delete data.second;
    }

    return it.value();
}

void CameraSettingsWidgetsCreator::cleanDataOnFail()
{
}

void CameraSettingsWidgetsCreator::treeWidgetItemSelectionChanged()
{
    const QList<QTreeWidgetItem*>& selectedItems = m_rootWidget.selectedItems();
    if( selectedItems.isEmpty() )
        return;

    Q_ASSERT( selectedItems.size() == 1 );
    QTreeWidgetItem* selectedItem = selectedItems.front();

    int ind = selectedItem->data(0, Qt::UserRole).toInt();
    if (ind + 1 > m_rootLayout.count()) {
        qDebug() << "CameraSettingsWidgetsCreator::treeWidgetItemSelectionChanged: index out of range! Ind = " << ind << ", Count = " << m_rootLayout.count();
        return;
    }

    m_rootLayout.setCurrentIndex(ind);

    emit refreshAdvancedSettings();
}

void CameraSettingsWidgetsCreator::parentOfRootElemFound(const QString& parentId)
{
    m_obj.parentOfRootElemFound(parentId);
}

//
// class CameraSettingTreeReader
//

template <class T, class E>
CameraSettingTreeReader<T, E>::CameraSettingTreeReader(
    const QString& id,
    const QnResourcePtr& cameraRes )
:
    m_initialId(id),
    m_currParentId(),
    m_firstTime(true),
    m_cameraRes(cameraRes)
{
    
}

template <class T, class E>
CameraSettingTreeReader<T, E>::~CameraSettingTreeReader()
{
    clean();
}

template <class T, class E>
void CameraSettingTreeReader<T, E>::parentOfRootElemFound(const QString& parentId)
{
    if (parentId.isEmpty()) {
        return;
    }

    m_currParentId = parentId;

    if (m_firstTime) {
        m_objList.push_back(createElement(parentId, m_cameraRes));
    }
}

template <class T, class E>
bool CameraSettingTreeReader<T, E>::proceed()
{
    if (m_firstTime) {
        parentOfRootElemFound(m_initialId);
    }

    QString currentId;
    m_currParentId = m_initialId;
    int i = 0;

    do
    {
        currentId = m_currParentId;

        T* nextElem = m_firstTime? m_objList.back(): m_objList.at(i++);
        //ToDo: check impossible situation i > size - 1
        if (!nextElem->proceed(getAdditionalInfo())) {
            clean();
            return false;
        }
    }
    while (currentId != m_currParentId);

    m_firstTime = false;
    return true;
}

template <class T, class E>
void CameraSettingTreeReader<T, E>::clean()
{
    foreach (T* obj, m_objList) {
        delete obj;
    }
    m_objList.clear();
}

//
// class CameraSettingsTreeLister
//

CameraSettingsTreeLister::CameraSettingsTreeLister(
    const QString& id,
    const QnResourcePtr& cameraRes )
:
    CameraSettingTreeReader<CameraSettingsLister, QSet<QString> >(id, cameraRes)
{
}

CameraSettingsLister* CameraSettingsTreeLister::createElement(
    const QString& id,
    const QnResourcePtr& cameraRes )
{
    return new CameraSettingsLister(id, *this, cameraRes);
}

QSet<QString>& CameraSettingsTreeLister::getAdditionalInfo()
{
    return m_params;
}

QStringList CameraSettingsTreeLister::proceed()
{
    m_params.clear();
    CameraSettingTreeReader<CameraSettingsLister, QSet<QString> >::proceed();
    return m_params.toList();
}

//
// class CameraSettingsWidgetsTreeCreator
//

CameraSettingsWidgetsTreeCreator::CameraSettingsWidgetsTreeCreator(
    const QString& cameraId,
    const QString& id,
#ifdef QT_WEBKITWIDGETS_LIB
    QWebView* webView,
#endif
    QTreeWidget& rootWidget,
    QStackedLayout& rootLayout
    )
:
    CameraSettingTreeReader<CameraSettingsWidgetsCreator, CameraSettings>(id),
#ifdef QT_WEBKITWIDGETS_LIB
    m_webView(webView),
#endif
    m_rootWidget(rootWidget),
    m_rootLayout(rootLayout),
    m_treeWidgetsById(),
    m_layoutIndById(),
    m_settingsWidgetsById(),
    m_settings(0),
    m_id(id),
    m_cameraId(cameraId)
{
    
}

CameraSettingsWidgetsTreeCreator::~CameraSettingsWidgetsTreeCreator()
{
    removeEmptyWidgetGroups();
    m_treeWidgetsById.clear();
    m_rootWidget.clear();
#ifdef QT_WEBKITWIDGETS_LIB
    m_webView = NULL;
#endif
}

CameraSettingsWidgetsCreator* CameraSettingsWidgetsTreeCreator::createElement(
    const QString& id,
    const QnResourcePtr& cameraRes )
{
    CameraSettingsWidgetsCreator* result = new CameraSettingsWidgetsCreator(id, *this, m_rootWidget, m_rootLayout, m_treeWidgetsById,
        m_layoutIndById, m_settingsWidgetsById, m_emptyGroupsById);
    result->setCamera( cameraRes );
    connect (result, &CameraSettingsWidgetsCreator::advancedParamChanged, this, &CameraSettingsWidgetsTreeCreator::advancedParamChanged);
    connect (result, &CameraSettingsWidgetsCreator::refreshAdvancedSettings, this, &CameraSettingsWidgetsTreeCreator::refreshAdvancedSettings);
    return result;
}

CameraSettings& CameraSettingsWidgetsTreeCreator::getAdditionalInfo()
{
    //ToDo: check pointer
    return *m_settings;
}

void CameraSettingsWidgetsTreeCreator::proceed(CameraSettings* settings)
{
    m_settings = settings;
    if ( !m_id.isNull() )
        CameraSettingTreeReader<CameraSettingsWidgetsCreator, CameraSettings>::proceed();
}

QString CameraSettingsWidgetsTreeCreator::getId() const
{
    return m_id;
}

QString CameraSettingsWidgetsTreeCreator::getCameraId() const
{
    return m_cameraId;
}

QTreeWidget* CameraSettingsWidgetsTreeCreator::getRootWidget()
{
    return &m_rootWidget;
}

QStackedLayout* CameraSettingsWidgetsTreeCreator::getRootLayout()
{
    return &m_rootLayout;
}

#ifdef QT_WEBKITWIDGETS_LIB
QWebView* CameraSettingsWidgetsTreeCreator::getWebView()
{
    return m_webView;
}
#endif

void CameraSettingsWidgetsTreeCreator::removeEmptyWidgetGroups()
{
    EmptyGroupsById::Iterator it = m_emptyGroupsById.begin();
    for (; it != m_emptyGroupsById.end(); ++it)
    {
        delete it.value();
    }
    m_emptyGroupsById.clear();
}
