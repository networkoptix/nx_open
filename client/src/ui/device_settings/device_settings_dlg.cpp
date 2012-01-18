#include "device_settings_dlg.h"

#include <QtGui/QBoxLayout>
#include <QtGui/QDialogButtonBox>
#include <QtGui/QGroupBox>
#include <QtGui/QTabWidget>

#include "device_settings_tab.h"
#include "settings.h"
#include "widgets.h"

CLAbstractDeviceSettingsDlg::CLAbstractDeviceSettingsDlg(QnResourcePtr resource, QWidget *parent)
    : QDialog(parent, Qt::CustomizeWindowHint | Qt::WindowTitleHint | Qt::WindowCloseButtonHint |
                      Qt::WindowStaysOnTopHint | Qt::MSWindowsFixedSizeDialogHint),
      m_resource(resource)
{
    setWindowTitle(tr("Camera settings: %1").arg(m_resource->toString()));

    resize(610, 490);

    m_tabWidget = new QTabWidget(this);

    foreach (const QString &group, m_resource->getResourceParamList().groupList())
        m_tabs.append(new CLDeviceSettingsTab(this, m_resource, group));

    m_buttonBox = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Reset | QDialogButtonBox::Close, Qt::Horizontal, this);

    QPushButton *suggestionsButton = m_buttonBox->addButton(tr("Suggestions..."), QDialogButtonBox::ActionRole);
    connect(suggestionsButton, SIGNAL(clicked()), this, SLOT(onSuggestions()));

    foreach (QAbstractButton *button, m_buttonBox->buttons())
        button->setFocusPolicy(Qt::NoFocus);

    connect(m_buttonBox, SIGNAL(accepted()), this, SLOT(accept()));
    connect(m_buttonBox, SIGNAL(rejected()), this, SLOT(reject()));
    connect(m_buttonBox->button(QDialogButtonBox::Reset), SIGNAL(clicked()), this, SLOT(reset()));


    QVBoxLayout *mainLayout = new QVBoxLayout;
    mainLayout->addWidget(m_tabWidget);
    mainLayout->addWidget(m_buttonBox);
    setLayout(mainLayout);

    connect(m_tabWidget, SIGNAL(currentChanged(int)), this, SLOT(onNewtab(int)));
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
    QDialog::accept();
}

void CLAbstractDeviceSettingsDlg::reject()
{
    QDialog::reject();
}

void CLAbstractDeviceSettingsDlg::reset()
{
}

void CLAbstractDeviceSettingsDlg::setParam(const QString &name, const QVariant &val)
{
    m_resource->setParamAsync(name, val, QnDomainPhysical);
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
    m_tabWidget->addTab(tab, tab->name());
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

QList<CLAbstractSettingsWidget *> CLAbstractDeviceSettingsDlg::widgets() const
{
    return m_widgets.values();
}

QList<CLAbstractSettingsWidget *> CLAbstractDeviceSettingsDlg::widgetsByGroup(const QString &group) const
{
    QList<CLAbstractSettingsWidget *> result;

    foreach (CLAbstractSettingsWidget *wgt, m_widgets.values()) {
        if (wgt->group() == group)
            result.append(wgt);
    }

    return result;
}

CLAbstractSettingsWidget *CLAbstractDeviceSettingsDlg::widgetByName(const QString &name) const
{
    return m_widgets.value(name);
}

void CLAbstractDeviceSettingsDlg::putWidget(CLAbstractSettingsWidget *widget)
{
    m_widgets.insert(widget->param().name(), widget);
}

void CLAbstractDeviceSettingsDlg::onSuggestions()
{
}

#include "core/resource/resource_command_consumer.h"

class QnDeviceGetParamCommand : public QnResourceCommand
{
public:
    QnDeviceGetParamCommand(CLAbstractSettingsWidget *wgt)
        : QnResourceCommand(wgt->resource()),
          m_wgt(wgt)
    {}

    void execute()
    {
        if (!isConnectedToTheResource())
            return;

        QVariant val;
        if (m_resource->getParam(m_wgt->param().name(), val, QnDomainPhysical))
            QMetaObject::invokeMethod(m_wgt, "updateParam", Qt::QueuedConnection, Q_ARG(QVariant, val));
    }

private:
    CLAbstractSettingsWidget *const m_wgt;
};

typedef QSharedPointer<QnDeviceGetParamCommand> QnDeviceGetParamCommandPtr;

void CLAbstractDeviceSettingsDlg::onNewtab(int index)
{
    const QString group = static_cast<CLDeviceSettingsTab *>(m_tabWidget->widget(index))->name();
    foreach (CLAbstractSettingsWidget *wgt, widgetsByGroup(group)) {
        QnDeviceGetParamCommandPtr command(new QnDeviceGetParamCommand(wgt));
        QnResource::addCommandToProc(command);
    }
}
