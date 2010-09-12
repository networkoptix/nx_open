#include "device_settings_dlg.h"
#include "device_settings_tab.h"
#include <QTabWidget>
#include <QDialogButtonBox>
#include <QVBoxLayout>
#include "../../device/device.h"

CLAbstractDeviceSettingsDlg::CLAbstractDeviceSettingsDlg(CLDevice* dev):
QDialog(0, Qt::CustomizeWindowHint | Qt::WindowTitleHint | 
		Qt::WindowCloseButtonHint| Qt::WindowStaysOnTopHint |
		Qt::MSWindowsFixedSizeDialogHint),
mDevice(dev)
{
	
	setWindowTitle(tr("Camera settings."));
	setWindowOpacity(0.85);

	int width = 640;
	int height = width*3/4;

	resize(width, height);

	QPalette palette;
	palette.setColor(backgroundRole(), Qt::black);
	setPalette(palette);

	mTabWidget = new QTabWidget;

	QList<QString> groups = mDevice->getDevicePramList().groupList();

	for (int i = 0; i < groups.count(); ++i)
	{
		QString group = groups.at(i);
		mTabWidget->addTab(new CLDeviceSettingsTab(mDevice, group), group);
	}


	mButtonBox = new QDialogButtonBox(QDialogButtonBox::Close);


	//! [4]
	QVBoxLayout *mainLayout = new QVBoxLayout;
	mainLayout->addWidget(mTabWidget);
	mainLayout->addWidget(mButtonBox);
	setLayout(mainLayout);



	init();

}

CLAbstractDeviceSettingsDlg::~CLAbstractDeviceSettingsDlg()
{

}
