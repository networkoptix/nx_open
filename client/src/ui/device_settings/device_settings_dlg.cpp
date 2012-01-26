#include "device_settings_dlg.h"

#include <QtGui/QBoxLayout>
#include <QtGui/QDialogButtonBox>
#include <QtGui/QGroupBox>
#include <QtGui/QMessageBox>
#include <QtGui/QTabWidget>

#include <api/AppServerConnection.h>

#include "device_settings_tab.h"

CLAbstractDeviceSettingsDlg::CLAbstractDeviceSettingsDlg(QnResourcePtr resource, QWidget *parent)
    : QDialog(parent, Qt::CustomizeWindowHint | Qt::WindowTitleHint | Qt::WindowCloseButtonHint),
      m_resource(resource), m_params(resource->getResourceParamList())
{
    setWindowTitle(tr("Camera settings: %1").arg(m_resource->getName()));

    resize(650, 500);

    m_tabWidget = new QTabWidget(this);
    connect(m_tabWidget, SIGNAL(currentChanged(int)), this, SLOT(currentTabChanged(int)));

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
    QnAppServerConnectionPtr appServerConnection = QnAppServerConnectionFactory::createConnection();
    if (appServerConnection->saveAsync(m_resource, this, SLOT(saveSuccess())) == 0)
        saveError();
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
    QList<QString> groups = tabsOrder();

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

    foreach (const QString &group, groups)
        addTab(new CLDeviceSettingsTab(this, m_params, group));

    reset();
    currentTabChanged(0); // QTabWidget doesn't emit currentChanged() for the first tab created
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
    if (QMessageBox::warning(this, tr("Unable to save changes"), tr("Please try save changes later."),
                             QMessageBox::Ok | QMessageBox::Close, QMessageBox::Ok) == QMessageBox::Close) {
        reject();
    }
}

QList<QString> CLAbstractDeviceSettingsDlg::tabsOrder() const
{
    QList<QString> tabsOrder;
    tabsOrder << QString(); // 'General' tab is a first tab;
    return tabsOrder;
}

QList<CLDeviceSettingsTab *> CLAbstractDeviceSettingsDlg::tabs() const
{
    return m_tabs;
}

CLDeviceSettingsTab *CLAbstractDeviceSettingsDlg::tabByName(const QString &name) const
{
    foreach (CLDeviceSettingsTab *tab, m_tabs) {
        if (name == tab->name())
            return tab;
    }

    return 0;
}

void CLAbstractDeviceSettingsDlg::addTab(CLDeviceSettingsTab *tab)
{
    m_tabs.append(tab);
    m_tabWidget->addTab(tab, !tab->name().isEmpty() ? tab->name() : tr("General"));
}

QList<QGroupBox *> CLAbstractDeviceSettingsDlg::groups() const
{
    return m_groups.values();
}

QGroupBox *CLAbstractDeviceSettingsDlg::groupByName(const QString &name) const
{
    return m_groups.value(name);
}

void CLAbstractDeviceSettingsDlg::putGroup(const QString &name, QGroupBox *group)
{
    m_groups.insert(name, group);
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
        const QByteArray signature = QByteArray("2") + QByteArray(mSignal.signature()); // a bit tricky, see Q_SIGNAL definition
        connect(widget, signature.constData(), this, SLOT(saveParam()));
    }
    m_widgets.insert(param.name(), widget);
}

QnParam CLAbstractDeviceSettingsDlg::param(QWidget *widget) const
{
    return m_params.value(m_widgets.key(widget));
}


#include "core/resource/resource_command_consumer.h"

class QnDeviceGetParamCommand : public QnResourceCommand
{
public:
    QnDeviceGetParamCommand(const QnResourcePtr &resource, QWidget *widget, const QnParam &param)
        : QnResourceCommand(resource),
          m_widget(widget), m_param(param)
    {}

    void execute()
    {
        if (!isConnectedToTheResource())
            return;

        QVariant value;
        if (m_resource->getParam(m_param.name(), value, m_param.isPhysical() ? QnDomainPhysical : QnDomainMemory)) {
            QMetaProperty userProperty = m_widget->metaObject()->userProperty();
            if (userProperty.isValid())
                userProperty.write(m_widget, value);
        }
    }

private:
    QWidget *const m_widget;
    const QnParam m_param;
};

typedef QSharedPointer<QnDeviceGetParamCommand> QnDeviceGetParamCommandPtr;

void CLAbstractDeviceSettingsDlg::currentTabChanged(int index)
{
    const QString group = static_cast<CLDeviceSettingsTab *>(m_tabWidget->widget(index))->name();
    foreach (const QnParam &param, m_params.paramList(group).list()) {
        if (QWidget *widget = m_widgets.value(param.name())) {
            QnDeviceGetParamCommandPtr command(new QnDeviceGetParamCommand(m_resource, widget, param));
            QnResource::addCommandToProc(command);
        }
    }
}
