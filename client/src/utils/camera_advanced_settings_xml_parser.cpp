#include "camera_advanced_settings_xml_parser.h"
#include "ui/widgets/properties/camera_advanced_settings_widget.h"

const QString CASXP_IMAGING_GROUP_NAME(QLatin1String("%%Imaging"));
const QString CASXP_MAINTENANCE_GROUP_NAME(QLatin1String("%%Maintenance"));

//
// class CameraSettingsLister
//

CameraSettingsLister::CameraSettingsLister(const QString& id, ParentOfRootElemFoundAware& obj):
    CameraSettingReader(id),
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

CameraSettingsWidgetsCreator::CameraSettingsWidgetsCreator(const QString& id, ParentOfRootElemFoundAware& obj,
        QTreeWidget& rootWidget, QStackedLayout& rootLayout, WidgetsById& widgetsById, QObject* handler):
    CameraSettingReader(id),
    m_obj(obj),
    m_settings(0),
    m_rootWidget(rootWidget),
    m_rootLayout(rootLayout),
    m_widgetsById(widgetsById),
    m_handler(handler),
    m_owner(0),
    m_emptyGroupsById(),
    m_layoutIndById()
{
    connect(&m_rootWidget, SIGNAL(itemPressed(QTreeWidgetItem*, int)), this,   SLOT(treeWidgetItemPressed(QTreeWidgetItem*, int)));
    connect(&m_rootWidget, SIGNAL(itemSelectionChanged()), this,   SLOT(treeWidgetItemSelectionChanged()));
}

CameraSettingsWidgetsCreator::~CameraSettingsWidgetsCreator()
{
    removeEmptyWidgetGroups();
}

void CameraSettingsWidgetsCreator::removeEmptyWidgetGroups()
{
    WidgetsAndParentsById::Iterator it = m_emptyGroupsById.begin();
    for (; it != m_emptyGroupsById.end(); ++it)
    {
        delete it.value();
    }
    m_emptyGroupsById.clear();
}

bool CameraSettingsWidgetsCreator::proceed(CameraSettings& settings)
{
    removeEmptyWidgetGroups();
    m_layoutIndById.clear();

    if (m_owner)
    {
        QObjectList children = m_owner->children();
        for (int i = 0; i < children.count(); ++i)
        {
            m_rootLayout.removeWidget(static_cast<QnAbstractSettingsWidget*>(children[i])->toWidget());
        }
        delete m_owner;
    }
    m_owner = new QWidget();

    m_settings = &settings;

    return read() && CameraSettingReader::proceed();
}

bool CameraSettingsWidgetsCreator::isGroupEnabled(const QString& id, const QString& parentId, const QString& name)
{
    if (m_widgetsById.find(id) != m_widgetsById.end()) {
        return true;
    }

    if (m_settings->find(id) != m_settings->end()) {
        //By default, group is enabled. It can be disabled by mediaserver
        //Disabled - means empty value. Group can have only empty value, so 
        //if we found it - it is disabled
        return false;
    }

    //QTreeWidgetItem* tabWidget = 0;
    /*QTreeWidgetItem* parentItem = 0;
    if (!parentId.isEmpty()) {
        WidgetsById::ConstIterator it = m_widgetsById.find(parentId);
        if (it == m_widgetsById.end())
        {
            parentItem = it.value();
        }
        else
        {
            //This should mean that parent is empty group
            WidgetsAndParentsById::ConstIterator eGroupsIt = m_emptyGroupsById.find(parentId);
            if (eGroupsIt == m_emptyGroupsById.end()) {
                //Parent must be already created!
                Q_ASSERT(false);
            }
            parentItem = eGroupsIt.value()->get();
        } 
    }*/

    QTreeWidgetItem* item = new QTreeWidgetItem((QTreeWidget*)0, QStringList(name));

    /**if (parentId.isEmpty())
    {
        m_rootWidget.addTopLevelItem(item);

        //tabWidget = new QWidget();
        //m_rootLayout.addWidget(tabWidget);

    } else {

        WidgetsById::ConstIterator it = m_widgetsById.find(parentId);
        if (it == m_widgetsById.end()) {
            //Parent must be already created!
            Q_ASSERT(false);
        }
        it.value()->addChild(item);
    }**/

    //item->setObjectName(id);
    item->setData(0, Qt::UserRole, 0);
    /**m_widgetsById.insert(id, item);**/
    m_emptyGroupsById.insert(id, new WidgetAndParent(item, parentId));

    return true;
}

bool CameraSettingsWidgetsCreator::isParamEnabled(const QString& id, const QString& /*parentId*/)
{
    if (m_widgetsById.find(id) != m_widgetsById.end()) {
        //Widget already created - so returning false
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
        //ToDo: uncomment and explore: qDebug() << "CameraSettingsWidgetsCreator::paramFound: didn't disable the following param: " << value.serializeToStr();
        return;
    }

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
    if (currIt != m_settings->end()) currIt.value()->setDescription(value.getDescription());

    QWidget* tabWidget = 0;
    switch(value.getType())
    {
        case CameraSetting::OnOff:
            tabWidget = new QnSettingsOnOffWidget(m_handler, *(currIt.value()), *rootWidget);
            break;

        case CameraSetting::MinMaxStep:
            tabWidget = new QnSettingsMinMaxStepWidget(m_handler, *(currIt.value()), *rootWidget);
            break;

        case CameraSetting::Enumeration:
            tabWidget = new QnSettingsEnumerationWidget(m_handler, *(currIt.value()), *rootWidget);
            break;

        case CameraSetting::Button:
            tabWidget = new QnSettingsButtonWidget(m_handler, value, *rootWidget);
            break;

        default:
            //Unknown widget type!
            Q_ASSERT(false);
    }
}

QTreeWidgetItem* CameraSettingsWidgetsCreator::findParentForParam(const QString& parentId)
{
    WidgetsById::ConstIterator it = m_widgetsById.find(parentId);
    if (it != m_widgetsById.end()) {
        return it.value();
    }

    QList<QPair<QString, WidgetAndParent*> > parentsHierarchy;

    QString grandParentId = parentId;
    while (true)
    {
        WidgetsAndParentsById::Iterator eGroupsIt = m_emptyGroupsById.find(grandParentId);
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
        it = m_widgetsById.insert(data.first, data.second->take());

        delete data.second;
    }
    else
    {
        it = m_widgetsById.find(grandParentId);
    }

    if (it == m_widgetsById.end()) {
        //Grand parent must exist!
        Q_ASSERT(false);
    }

    while (!parentsHierarchy.isEmpty())
    {
        QPair<QString, WidgetAndParent*> data = parentsHierarchy.takeFirst();
        it.value()->addChild(data.second->get());
        it = m_widgetsById.insert(data.first, data.second->take());

        delete data.second;
    }

    return it.value();
}

void CameraSettingsWidgetsCreator::cleanDataOnFail()
{
    
}

void CameraSettingsWidgetsCreator::treeWidgetItemPressed(QTreeWidgetItem* item, int /*column*/)
{
    if (!item) {
        return;
    }

    int ind = item->data(0, Qt::UserRole).toInt();
    if (ind + 1 > m_rootLayout.count()) {
        qDebug() << "CameraSettingsWidgetsCreator::treeWidgetItemPressed: index out of range! Ind = " << ind << ", Count = " << m_rootLayout.count();
        return;
    }

    m_rootLayout.setCurrentIndex(ind);
}

void CameraSettingsWidgetsCreator::treeWidgetItemSelectionChanged()
{
    if (m_rootLayout.count() > 0) {
        m_rootLayout.setCurrentIndex(0);
    }
}

void CameraSettingsWidgetsCreator::parentOfRootElemFound(const QString& parentId)
{
    m_obj.parentOfRootElemFound(parentId);
}

//
// class CameraSettingTreeReader
//

template <class T, class E>
CameraSettingTreeReader<T, E>::CameraSettingTreeReader(const QString& id):
    m_initialId(id),
    m_currParentId(),
    m_firstTime(true)
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
        m_objList.push_back(createElement(parentId));
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
        if (!nextElem->proceed(getCallback())) {
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

CameraSettingsTreeLister::CameraSettingsTreeLister(const QString& id):
    CameraSettingTreeReader<CameraSettingsLister, QSet<QString> >(id)
{
    
}

CameraSettingsLister* CameraSettingsTreeLister::createElement(const QString& id)
{
    return new CameraSettingsLister(id, *this);
}

QSet<QString>& CameraSettingsTreeLister::getCallback()
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

CameraSettingsWidgetsTreeCreator::CameraSettingsWidgetsTreeCreator(const QString& id, QTreeWidget& rootWidget,
        QStackedLayout& rootLayout, QObject* handler):
    CameraSettingTreeReader<CameraSettingsWidgetsCreator, CameraSettings>(id),
    m_rootWidget(rootWidget),
    m_rootLayout(rootLayout),
    m_widgetsById(),
    m_handler(handler),
    m_settings(0)
{
    //Default - show empty widget
    m_rootLayout.addWidget(new QWidget());
}

CameraSettingsWidgetsCreator* CameraSettingsWidgetsTreeCreator::createElement(const QString& id)
{
    return new CameraSettingsWidgetsCreator(id, *this, m_rootWidget, m_rootLayout, m_widgetsById, m_handler);
}

CameraSettings& CameraSettingsWidgetsTreeCreator::getCallback()
{
    //ToDo: check pointer
    return *m_settings;
}

void CameraSettingsWidgetsTreeCreator::proceed(CameraSettings* settings)
{
    m_settings = settings;

    m_widgetsById.clear();
    m_rootWidget.clear();

    CameraSettingTreeReader<CameraSettingsWidgetsCreator, CameraSettings>::proceed();
}
