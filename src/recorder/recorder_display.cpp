#include "recorder_display.h"
#include "recorder_device.h"
#include "ui\videoitem\recorder_item.h"
#include "base\sleep.h"

CLRecorderDisplay::CLRecorderDisplay(CLDevice* dev, CLRecorderItem* recitem):
mDev(dev),
mRecitem(recitem)
{
	recitem->setComplicatedItem(this);
}

CLRecorderDisplay::~CLRecorderDisplay()
{
	stop();
}

CLDevice* CLRecorderDisplay::getDevice() const
{
	return mDev;
}

void CLRecorderDisplay::startDispay()
{
	start();
}

void CLRecorderDisplay::beforestopDispay()
{
	pleaseStop();
}

void CLRecorderDisplay::stopDispay()
{
	stop();
}

CLAbstractSceneItem* CLRecorderDisplay::getSceneItem() const
{
	return mRecitem;
}

CLRecorderItem* CLRecorderDisplay::getRecorderItem() const
{
	return mRecitem;
}

void CLRecorderDisplay::run()
{
	while(!needToStop())
	{
		QString text = mDev->toString();
		mRecitem->setText(text);

		//=== 0.5 sec delay===
		for (int i = 0; i < 5; ++i)
		{
			if (needToStop())
				break;

			CLSleep::msleep(100);
		}

	}
}
