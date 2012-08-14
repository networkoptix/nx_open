#include "camera_advanced_settings_xml_parser.h"
#include "ui/widgets/properties/camera_advanced_settings_widget.h"

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

bool CameraSettingsLister::isGroupEnabled(const QString& id, const QString& /*parentId*/, const QString& /*name*/)
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

CameraSettingsWidgetsCreator::CameraSettingsWidgetsCreator(const QString& filepath, QTreeWidget& rootWidget, QStackedLayout& rootLayout, QObject* handler):
    CameraSettingReader(filepath, QString::fromLatin1("ONVIF")),
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

bool CameraSettingsWidgetsCreator::recreateWidgets(CameraSettings* settings)
{
    if (!settings) {
        return false;
    }

    m_settings = settings;

    //WidgetsById::Iterator it = m_widgetsById.begin();
    //for (; it != m_widgetsById.end(); ++it)
    //{
    //    //Deleting root element (automaticaly all children will be deleted)
    //    if (it.key().count(QString::fromLatin1("%%")) == 1)
    //    {
    //        delete it.value();
    //    }
    //}
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

    return read() && proceed();
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
        //QVBoxLayout *layout = dynamic_cast<QVBoxLayout *>(it.value()->layout());
        //if(!layout) {
        //    delete it.value()->layout();
        //    it.value()->setLayout(layout = new QVBoxLayout());
        //}

        //tabWidget = new QGroupBox(name);
        //layout->addWidget(tabWidget);

        //tabWidget->setFixedWidth(300);
        //tabWidget->setFixedHeight(300);
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

    //m_widgetsById.insert(value.getId(), tabWidget);
}

void CameraSettingsWidgetsCreator::cleanDataOnFail()
{
    
}

void CameraSettingsWidgetsCreator::treeWidgetItemPressed(QTreeWidgetItem* item, int /*column*/)
{
    if (!item) {
        return;
    }

    unsigned int ind = item->data(0, Qt::UserRole).toUInt();
    if (ind > m_rootLayout.count() - 1) {
        qDebug() << "CameraSettingsWidgetsCreator::treeWidgetItemPressed: index out of range! Ind = " << ind << ", Count = " << m_rootLayout.count();
        return;
    }

    m_rootLayout.setCurrentIndex(ind);
}
