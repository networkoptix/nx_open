#include "device_settings_dlg.h"
#include "device_settings_tab.h"
#include <QTabWidget>
#include <QDialogButtonBox>
#include <QVBoxLayout>
#include "..\src\gui\widgets\qtabbar.h"

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

	setStyleSheet("QPushButton {border-style: solid;		border-color: green;	background-color: blue;}");
	setStyleSheet("QTabBar::tab {		border-width: 10px;		border-style: solid;		border-color: green;	background-color: pink;}");

	mTabWidget = new QTabWidget;
	mTabWidget->setPalette(palette);
	mTabWidget->setAutoFillBackground(true);
	//mTabWidget->tabBar();
	//mTabWidget->setStyleSheet("QTabBar { color: red }");
	//mTabWidget->setStyleSheet("* { color: red }");

	mTabWidget->setStyleSheet("QTabBar::tab {		border-width: 1px;		border-style: solid;		border-color: green;	background-color: pink;}");
	
	QTabBar* bar = new QTabBar;
	bar->addTab("ttt");


	mTabWidget->addTab(new CLDeviceSettingsTab(), tr("General"));
	mTabWidget->addTab(new CLDeviceSettingsTab(), tr("Image"));
	

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
