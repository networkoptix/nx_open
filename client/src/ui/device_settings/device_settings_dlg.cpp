#include "device_settings_dlg.h"

#include <QtGui/QBoxLayout>
#include <QtGui/QDialogButtonBox>
#include <QtGui/QGroupBox>
#include <QtGui/QTabWidget>

#include "device_settings_tab.h"
#include "settings.h"
#include "settings_getter.h"
#include "widgets.h"

CLAbstractDeviceSettingsDlg::CLAbstractDeviceSettingsDlg(QnResourcePtr resource)
    : QDialog(0, Qt::CustomizeWindowHint | Qt::WindowTitleHint | Qt::WindowCloseButtonHint |
                 Qt::WindowStaysOnTopHint | Qt::MSWindowsFixedSizeDialogHint),
        m_resource(resource)
{
    setWindowTitle(tr("Camera settings: ") + m_resource->toString());
    //setWindowOpacity(global_dlg_opacity);

    int width = 610;
    int height = 490;

    resize(width, height);

    //QPalette pal = palette();
    //pal.setColor(backgroundRole(), Qt::black);
    //setPalette(pal);

    m_tabWidget = new QTabWidget(this);

    QList<QString> groups = m_resource->getResourceParamList().groupList();

    foreach (const QString &group, m_resource->getResourceParamList().groupList())
        m_tabs.append(new CLDeviceSettingsTab(this, this, m_resource, group));

    m_buttonBox = new QDialogButtonBox(this);
    m_buttonBox->setFocusPolicy(Qt::NoFocus);

    QPushButton* suggestionsBtn = new QPushButton(tr("Suggestions..."));
    connect(suggestionsBtn, SIGNAL(released()), this, SLOT(onSuggestions()));
    m_buttonBox->addButton(suggestionsBtn, QDialogButtonBox::ActionRole);
    suggestionsBtn->setFocusPolicy(Qt::NoFocus);

    QPushButton* closeBtn = new QPushButton(tr("Close"));
    connect(closeBtn, SIGNAL(released()), this, SLOT(onClose()));
    m_buttonBox->addButton(closeBtn, QDialogButtonBox::RejectRole);
    closeBtn->setFocusPolicy(Qt::NoFocus);

    QVBoxLayout *mainLayout = new QVBoxLayout;
    mainLayout->addWidget(m_tabWidget);
    mainLayout->addWidget(m_buttonBox);
    setLayout(mainLayout);

    //suggestionsBtn->move(30,30);

    connect(m_tabWidget, SIGNAL(currentChanged(int)), this, SLOT(onNewtab(int)));
}

CLAbstractDeviceSettingsDlg::~CLAbstractDeviceSettingsDlg()
{
    qDeleteAll(mWgtsLst);
}

QnResourcePtr CLAbstractDeviceSettingsDlg::resource() const
{
    return m_resource;
}

void CLAbstractDeviceSettingsDlg::setParam(const QString &name, const QVariant &val)
{
    m_resource->setParamAsynch(name, val, QnDomainPhysical);
}

void CLAbstractDeviceSettingsDlg::addTab(CLDeviceSettingsTab *tab)
{
    m_tabWidget->addTab(tab, tab->name());
}

CLDeviceSettingsTab *CLAbstractDeviceSettingsDlg::tabByName(const QString &name) const
{
    foreach (CLDeviceSettingsTab *tab, m_tabs)
    {
        if (name == tab->name())
            return tab;
    }

    return 0;
}

void CLAbstractDeviceSettingsDlg::putWidget(CLAbstractSettingsWidget *wgt)
{
    mWgtsLst.append(wgt);
}

CLAbstractSettingsWidget *CLAbstractDeviceSettingsDlg::getWidgetByName(const QString &name) const
{
    foreach (CLAbstractSettingsWidget *wgt, mWgtsLst)
    {
        if (name == wgt->param().name())
            return wgt;
    }

    return 0;
}

void CLAbstractDeviceSettingsDlg::putGroup(QGroupBox *group)
{
    m_groups.append(group);
}

QGroupBox *CLAbstractDeviceSettingsDlg::getGroupByName(const QString &name) const
{
    foreach (QGroupBox *groupBox, m_groups)
    {
        if (name == groupBox->title())
            return groupBox;
    }

    return 0;
}

QList<CLAbstractSettingsWidget *> CLAbstractDeviceSettingsDlg::getWidgetsBygroup(const QString &group) const
{
    QList<CLAbstractSettingsWidget *> result;

    foreach (CLAbstractSettingsWidget *wgt, mWgtsLst)
    {
        if (wgt->group() == group)
            result.append(wgt);
    }

    return result;
}

void CLAbstractDeviceSettingsDlg::onClose()
{
    close();
}

void CLAbstractDeviceSettingsDlg::onSuggestions()
{
}

void CLAbstractDeviceSettingsDlg::onNewtab(int index)
{
    QString group = static_cast<CLDeviceSettingsTab *>(m_tabWidget->widget(index))->name();
    foreach (CLAbstractSettingsWidget *wgt, getWidgetsBygroup(group))
    {
        QnDeviceGetParamCommandPtr command(new QnDeviceGetParamCommand(wgt));
        QnResource::addCommandToProc(command);
    }
}
