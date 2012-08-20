#include "camera_advanced_settings_xml_parser.h"
#include "ui/widgets/properties/camera_advanced_settings_widget.h"

const QString CASXP_IMAGING_GROUP_NAME(QLatin1String("%%Imaging"));
const QString CASXP_MAINTENANCE_GROUP_NAME(QLatin1String("%%Maintenance"));

//
// class CameraSettingsLister
//

CameraSettingsLister::CameraSettingsLister(const QString& filepath, const QString& id, ParentOfRootElemFoundAware& obj):
    CameraSettingReader(filepath, id),
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

CameraSettingsWidgetsCreator::CameraSettingsWidgetsCreator(const QString& filepath, const QString& id, ParentOfRootElemFoundAware& obj,
        QTreeWidget& rootWidget, QStackedLayout& rootLayout, QObject* handler):
    CameraSettingReader(filepath, id),
    m_obj(obj),
    m_settings(0),
    m_rootWidget(rootWidget),
    m_rootLayout(rootLayout),
    m_handler(handler),
    m_owner(0)
{
    connect(&m_rootWidget, SIGNAL(itemPressed(QTreeWidgetItem*, int)), this,   SLOT(treeWidgetItemPressed(QTreeWidgetItem*, int)));
}

CameraSettingsWidgetsCreator::~CameraSettingsWidgetsCreator()
{

}

bool CameraSettingsWidgetsCreator::proceed(CameraSettings& settings)
{
    m_settings = &settings;

    m_widgetsById.clear();
    m_rootWidget.clear();

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

    //Default - show empty widget
    m_rootLayout.addWidget(new QWidget(m_owner));

    return read() && CameraSettingReader::proceed();
}

bool CameraSettingsWidgetsCreator::isGroupEnabled(const QString& id, const QString& parentId, const QString& name)
{
    if (m_settings->find(id) != m_settings->end()) {
        //By default, group is enabled. It can be disabled by mediaserver
        //Disabled - means empty value. Group can have only empty value, so 
        //if we found it - it is disabled
        return false;
    }

    //QTreeWidgetItem* tabWidget = 0;
    QTreeWidgetItem* item = new QTreeWidgetItem((QTreeWidget*)0, QStringList(name));

    if (parentId.isEmpty())
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
    }

    //item->setObjectName(id);
    item->setData(0, Qt::UserRole, 0);
    m_widgetsById.insert(id, item);

    return true;
}

bool CameraSettingsWidgetsCreator::isParamEnabled(const QString& id, const QString& /*parentId*/)
{
    CameraSettings::ConstIterator it = m_settings->find(id);
    if (it == m_settings->end()) {
        //It means enabled, but have no value
        return true;
    }

    return isEnabled(*(it.value()));
}

void CameraSettingsWidgetsCreator::paramFound(const CameraSetting& value, const QString& parentId)
{
    WidgetsById::ConstIterator parentIt = m_widgetsById.find(parentId);
    if (parentIt == m_widgetsById.end()) {
        //Parent must be already created!
        Q_ASSERT(false);
    }

    CameraSettings::Iterator currIt = m_settings->find(value.getId());
    if (currIt == m_settings->end() && value.getType() != CameraSetting::Button) {
        //ToDo: uncomment and explore: qDebug() << "CameraSettingsWidgetsCreator::paramFound: didn't disable the following param: " << value.serializeToStr();
        return;
    }

    QWidget* tabWidget = 0;
    switch(value.getType())
    {
        case CameraSetting::OnOff:
            tabWidget = new QnSettingsOnOffWidget(m_handler, *(currIt.value()), *m_owner);
            break;

        case CameraSetting::MinMaxStep:
            tabWidget = new QnSettingsMinMaxStepWidget(m_handler, *(currIt.value()), *m_owner);
            break;

        case CameraSetting::Enumeration:
            tabWidget = new QnSettingsEnumerationWidget(m_handler, *(currIt.value()), *m_owner);
            break;

        case CameraSetting::Button:
            tabWidget = new QnSettingsButtonWidget(m_handler, value, *m_owner);
            break;

        default:
            //Unknown widget type!
            Q_ASSERT(false);
    }

    int ind = m_rootLayout.addWidget(tabWidget);

    QTreeWidgetItem* item = new QTreeWidgetItem((QTreeWidget*)0, QStringList(value.getName()));
    item->setData(0, Qt::UserRole, ind);
    parentIt.value()->addChild(item);
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

CameraSettingsTreeLister::CameraSettingsTreeLister(const QString& filepath, const QString& id):
    CameraSettingTreeReader<CameraSettingsLister, QSet<QString> >(id),
    m_filepath(filepath)
{
    
}

CameraSettingsLister* CameraSettingsTreeLister::createElement(const QString& id)
{
    return new CameraSettingsLister(m_filepath, id, *this);
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

CameraSettingsWidgetsTreeCreator::CameraSettingsWidgetsTreeCreator(const QString& filepath, const QString& id, QTreeWidget& rootWidget,
        QStackedLayout& rootLayout, QObject* handler):
    CameraSettingTreeReader<CameraSettingsWidgetsCreator, CameraSettings>(id),
    m_filepath(filepath),
    m_rootWidget(rootWidget),
    m_rootLayout(rootLayout),
    m_handler(handler),
    m_settings(0)
{

}

CameraSettingsWidgetsCreator* CameraSettingsWidgetsTreeCreator::createElement(const QString& id)
{
    return new CameraSettingsWidgetsCreator(m_filepath, id, *this, m_rootWidget, m_rootLayout, m_handler);
}

CameraSettings& CameraSettingsWidgetsTreeCreator::getCallback()
{
    //ToDo: check pointer
    return *m_settings;
}

void CameraSettingsWidgetsTreeCreator::proceed(CameraSettings* settings)
{
    m_settings = settings;
    CameraSettingTreeReader<CameraSettingsWidgetsCreator, CameraSettings>::proceed();
}
