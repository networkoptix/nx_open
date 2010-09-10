#include "device_settings_tab.h"
#include <QVBoxLayout>
#include <QPushButton>

CLDeviceSettingsTab::CLDeviceSettingsTab()
{
	setAutoFillBackground(true);
	QPalette palette;
	palette.setColor(backgroundRole(), Qt::black);
	setPalette(palette);

	//setStyleSheet("* { color: red }");
	
	
	QVBoxLayout *mainLayout = new QVBoxLayout;
	mainLayout->addWidget(new QPushButton("btn", this));
	setLayout(mainLayout);
}

CLDeviceSettingsTab::~CLDeviceSettingsTab()
{

}
