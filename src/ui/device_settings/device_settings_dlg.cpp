#include "device_settings_dlg.h"
#include "device_settings_tab.h"
#include "../../device/device.h"
#include "style.h"
#include "widgets.h"
#include "settings.h"
#include "settings_getter.h"

CLAbstractDeviceSettingsDlg::CLAbstractDeviceSettingsDlg(CLDevice* dev):
QDialog(0, Qt::CustomizeWindowHint | Qt::WindowTitleHint | 
		Qt::WindowCloseButtonHint| Qt::WindowStaysOnTopHint |
		Qt::MSWindowsFixedSizeDialogHint),
mDevice(dev)
{
	QString str = tr("Camera settings");
	str+=": ";
	str+=mDevice->toString();

	setWindowTitle(str);
	//setWindowOpacity(global_dlg_opacity);

	int width = 610;
	int height = 490;

	resize(width, height);

	QPalette palette;
	palette.setColor(backgroundRole(), Qt::black);
	setPalette(palette);

	QStyle *arthurStyle = new ArthurStyle();
	setStyle(arthurStyle);

	QVBoxLayout *mainLayout = new QVBoxLayout;

	mTabWidget = new QTabWidget;

	QList<QString> groups = mDevice->getDeviceParamList().groupList();

	for (int i = 0; i < groups.count(); ++i)
	{
		QString group = groups.at(i);
		mTabs.push_back(new CLDeviceSettingsTab(this, this, mDevice, group));
	}

	mButtonBox = new QDialogButtonBox();
	mButtonBox->setFocusPolicy(Qt::NoFocus);

	QPushButton* suggestionsBtn = new QPushButton("Suggestions...");
	connect(suggestionsBtn, SIGNAL(released()), this, SLOT(onSuggestions()));
	mButtonBox->addButton(suggestionsBtn, QDialogButtonBox::ActionRole);
	suggestionsBtn->setFocusPolicy(Qt::NoFocus);

	QPushButton* closeBtn = new QPushButton("Close");
	connect(closeBtn, SIGNAL(released()), this, SLOT(onClose()));
	mButtonBox->addButton(closeBtn, QDialogButtonBox::RejectRole);
	closeBtn->setFocusPolicy(Qt::NoFocus);

	//! [4]

	mainLayout->addWidget(mTabWidget);
	mainLayout->addWidget(mButtonBox);
	setLayout(mainLayout);

	//suggestionsBtn->move(30,30);
	mDevice->addRef();

    connect(mTabWidget, SIGNAL(currentChanged(int)), this, SLOT(onNewtab(int)) );

}

CLAbstractDeviceSettingsDlg::~CLAbstractDeviceSettingsDlg()
{
	for (int i = 0; i < mWgtsLst.count(); ++i)
	{
		CLAbstractSettingsWidget* wgt = mWgtsLst.at(i);
		delete wgt;
	}

	mDevice->releaseRef();
}

CLDevice* CLAbstractDeviceSettingsDlg::getDevice() const
{
	return mDevice;
}

void CLAbstractDeviceSettingsDlg::setParam(const QString& name, const CLValue& val)
{
	mDevice->setParam_asynch(name,val);
}

void CLAbstractDeviceSettingsDlg::addTabWidget(CLDeviceSettingsTab* tab)
{
	mTabWidget->addTab(tab, tab->name());
}

CLDeviceSettingsTab* CLAbstractDeviceSettingsDlg::tabByName(QString name) const
{
	for (int i = 0; i < mTabs.count(); ++i)
	{
		CLDeviceSettingsTab* tab = mTabs.at(i);
		if (name == tab->name())
			return tab;
	}

	return 0;
}

void CLAbstractDeviceSettingsDlg::putWidget(CLAbstractSettingsWidget* wgt)
{
	mWgtsLst.push_back(wgt);
}

CLAbstractSettingsWidget* CLAbstractDeviceSettingsDlg::getWidgetByName(QString name) const
{
	for (int i = 0; i < mWgtsLst.count(); ++i)
	{
		CLAbstractSettingsWidget* wgt = mWgtsLst.at(i);
		if (name == wgt->param().name)
			return wgt;
	}

	return 0;
}

void CLAbstractDeviceSettingsDlg::putGroup(QGroupBox* group)
{
	mGroups.push_back(group);
}

QGroupBox* CLAbstractDeviceSettingsDlg::getGroupByName(QString name) const
{
	for (int i = 0; i < mGroups.count(); ++i)
	{
		QGroupBox* wgt = mGroups.at(i);
		if (name == wgt->title())
			return wgt;
	}
	return 0;
}

QList<CLAbstractSettingsWidget*> CLAbstractDeviceSettingsDlg::getWidgetsBygroup(QString group) const
{
    QList<CLAbstractSettingsWidget*> result;

    foreach(CLAbstractSettingsWidget* wgt, mWgtsLst)
    {
        if (wgt->group()==group)
            result.push_back(wgt);
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
    QString group = static_cast<CLDeviceSettingsTab*>(mTabWidget->currentWidget())->name();
    QList<CLAbstractSettingsWidget*> wgt_to_update = getWidgetsBygroup(group);

    foreach(CLAbstractSettingsWidget* wgt, wgt_to_update)
    {
        CLDeviceGetParamCommand* command = new CLDeviceGetParamCommand(wgt);
        CLDevice::addCommandToProc(command);
        command->releaseRef();
    }

}
