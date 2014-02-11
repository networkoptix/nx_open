#include "device_settings_dlg.h"

#include <QtCore/QMetaProperty>
#include <QtWidgets/QAbstractButton>
#include <QtWidgets/QBoxLayout>
#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QGroupBox>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QTabWidget>

#include <api/app_server_connection.h>

#include "device_settings_tab.h"

CLAbstractDeviceSettingsDlg::CLAbstractDeviceSettingsDlg(QnResourcePtr resource, QWidget *parent)
    : QDialog(parent, Qt::CustomizeWindowHint | Qt::WindowTitleHint | Qt::WindowCloseButtonHint),
      m_resource(resource), m_params(resource->getResourceParamList())
{
    setWindowTitle(tr("Camera settings: %1").arg(m_resource->getName()));

    resize(650, 500);

    m_tabWidget = new QTabWidget(this);

    m_buttonBox = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Reset | QDialogButtonBox::Close, Qt::Horizontal, this);
    foreach (QAbstractButton *button, m_buttonBox->buttons())
        button->setFocusPolicy(Qt::NoFocus);

    connect(m_buttonBox, SIGNAL(accepted()), this, SLOT(accept()));
    connect(m_buttonBox, SIGNAL(rejected()), this, SLOT(reject()));
    connect(m_buttonBox->button(QDialogButtonBox::Reset), SIGNAL(clicked()), this, SLOT(reset()));


    QVBoxLayout *mainLayout = new QVBoxLayout;
    mainLayout->addWidget(m_tabWidget);
    mainLayout->addWidget(m_buttonBox);
    setLayout(mainLayout);

    QMetaObject::invokeMethod(this, "buildTabs", Qt::QueuedConnection);
}

CLAbstractDeviceSettingsDlg::~CLAbstractDeviceSettingsDlg()
{
}

QnResourcePtr CLAbstractDeviceSettingsDlg::resource() const
{
    return m_resource;
}

void CLAbstractDeviceSettingsDlg::accept()
{
    if( QnAppServerConnectionFactory::getConnection2()->getResourceManager()->save(
            m_resource, this, &CLAbstractDeviceSettingsDlg::saveSuccess ) == ec2::INVALID_REQ_ID )
    {
        saveError();
    }
}

void CLAbstractDeviceSettingsDlg::reject()
{
    QDialog::reject();
}

void CLAbstractDeviceSettingsDlg::reset()
{
    foreach (const QnParam &param, m_params.list()) {
        if (QWidget *widget = m_widgets.value(param.name())) {
            QMetaProperty userProperty = widget->metaObject()->userProperty();
            if (userProperty.isValid() && userProperty.isWritable())
                userProperty.write(widget, param.value());
        }
    }
}

void CLAbstractDeviceSettingsDlg::buildTabs()
{
    buildTabs(m_tabWidget, QList<QString>() << QString());
}

void CLAbstractDeviceSettingsDlg::buildTabs(QTabWidget *tabWidget, const QList<QString> &tabsOrder)
{
    connect(tabWidget, SIGNAL(currentChanged(int)), this, SLOT(currentTabChanged(int)));
    int fakeTabIndex = tabWidget->count() == 0 ? tabWidget->addTab(new QWidget(tabWidget), QString()) : -1;

    QList<QString> groups = tabsOrder;

    const QList<QString> paramsGroupList = m_params.groupList();
    for (QList<QString>::iterator it = groups.begin(); it != groups.end(); ) {
        if (!(*it).isEmpty() && !paramsGroupList.contains(*it))
            it = groups.erase(it);
        else
            ++it;
    }
    foreach (const QString &group, paramsGroupList) {
        if (!group.isEmpty() && !groups.contains(group))
            groups << group;
    }

    foreach (const QString &group, groups) {
        CLDeviceSettingsTab *tab = new CLDeviceSettingsTab(this, m_params, group);
        tabWidget->addTab(tab, !group.isEmpty() ? group : tr("General"));
        addTab(tab, group);
    }

    reset();

    if (fakeTabIndex != -1)
        tabWidget->removeTab(fakeTabIndex); // QTabWidget doesn't emit currentChanged() for the first tab created
}

void CLAbstractDeviceSettingsDlg::setParam(const QString &paramName, const QVariant &value)
{
    const QnParam param = m_params.value(paramName);
    m_resource->setParamAsync(paramName, value, param.isPhysical() ? QnDomainPhysical : QnDomainMemory);
}

void CLAbstractDeviceSettingsDlg::saveParam()
{
    if (QWidget *widget = qobject_cast<QWidget *>(sender())) {
        const QString paramName = m_widgets.key(widget);
        if (!paramName.isEmpty()) {
            const QnParam param = m_params.value(paramName);
            QVariant value;
            QMetaProperty userProperty = widget->metaObject()->userProperty();
            if (userProperty.isValid()) {
                value = userProperty.read(widget);
                m_resource->setParamAsync(paramName, value, param.isPhysical() ? QnDomainPhysical : QnDomainMemory);
            }
        }
    }
}

void CLAbstractDeviceSettingsDlg::saveSuccess()
{
    QDialog::accept();
}

void CLAbstractDeviceSettingsDlg::saveError()
{
    if (QMessageBox::warning(this, tr("Unable to save changes"), tr("Please try to save changes later."), QMessageBox::Ok | QMessageBox::Close, QMessageBox::Ok) == QMessageBox::Close)
        reject();
}

QList<CLDeviceSettingsTab *> CLAbstractDeviceSettingsDlg::tabs() const
{
    return m_tabs.values();
}

CLDeviceSettingsTab *CLAbstractDeviceSettingsDlg::tabByName(const QString &name) const
{
    return m_tabs.value(name.toLower());
}

void CLAbstractDeviceSettingsDlg::addTab(CLDeviceSettingsTab *tab, const QString &name)
{
    m_tabs.insert(name.toLower(), tab);
}

QList<QGroupBox *> CLAbstractDeviceSettingsDlg::groups() const
{
    return m_groups.values();
}

QGroupBox *CLAbstractDeviceSettingsDlg::groupByName(const QString &name) const
{
    return m_groups.value(name.toLower());
}

void CLAbstractDeviceSettingsDlg::putGroup(const QString &name, QGroupBox *group)
{
    m_groups.insert(name.toLower(), group);
}

QList<QWidget *> CLAbstractDeviceSettingsDlg::widgets() const
{
    return m_widgets.values();
}

QList<QWidget *> CLAbstractDeviceSettingsDlg::widgetsByGroup(const QString &group, const QString &subGroup) const
{
    QList<QWidget *> widgets;
    foreach (const QnParam &param, m_params.paramList(group, subGroup).list()) {
        if (m_widgets.contains(param.name()))
            widgets.append(m_widgets[param.name()]);
    }

    return widgets;
}

QWidget *CLAbstractDeviceSettingsDlg::widgetByName(const QString &name) const
{
    return m_widgets.value(name);
}

void CLAbstractDeviceSettingsDlg::registerWidget(QWidget *widget, const QnParam &param)
{
    if (!widget || !param.isValid())
        return;

    QMetaProperty userProperty = widget->metaObject()->userProperty();
    if (userProperty.isValid() && userProperty.hasNotifySignal()) {
        const QMetaMethod mSignal = userProperty.notifySignal();
        const QByteArray signature = QByteArray("2") + QByteArray(mSignal.methodSignature()); // a bit tricky, see Q_SIGNAL definition
        connect(widget, signature.constData(), this, SLOT(saveParam()));
    }
    m_widgets.insert(param.name(), widget);
}

QnParam CLAbstractDeviceSettingsDlg::param(QWidget *widget) const
{
    return m_params.value(m_widgets.key(widget));
}


#include "core/resource/resource_command_processor.h"

class QnDeviceGetParamCommand : public QnResourceCommand
{
public:
    QnDeviceGetParamCommand(const QnResourcePtr &resource, QWidget *widget, const QnParam &param)
        : QnResourceCommand(resource),
          m_widget(widget), m_param(param)
    {}

    bool execute()
    {
        if (!isConnectedToTheResource())
            return false;

        QVariant value;
        if (m_resource->getParam(m_param.name(), value, m_param.isPhysical() ? QnDomainPhysical : QnDomainMemory)) 
        {
            QMetaProperty userProperty = m_widget->metaObject()->userProperty();
            if (userProperty.isValid())
                userProperty.write(m_widget, value);
        }
        else
            return false;

        return true;
    }

private:
    QWidget *const m_widget;
    const QnParam m_param;
};

typedef QSharedPointer<QnDeviceGetParamCommand> QnDeviceGetParamCommandPtr;

void CLAbstractDeviceSettingsDlg::currentTabChanged(int index)
{
    QTabWidget *tabWidget = qobject_cast<QTabWidget *>(sender());
    if (!tabWidget)
        return;

    CLDeviceSettingsTab *tab = qobject_cast<CLDeviceSettingsTab *>(tabWidget->widget(index));
    if (!tab)
        return;

    const QString group = m_tabs.key(tab);
    foreach (const QnParam &param, m_params.paramList(group).list()) {
        if (QWidget *widget = m_widgets.value(param.name())) {
            QnDeviceGetParamCommandPtr command(new QnDeviceGetParamCommand(m_resource, widget, param));
            QnResource::addCommandToProc(command);
        }
    }
}
