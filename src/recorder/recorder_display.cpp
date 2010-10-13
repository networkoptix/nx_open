#include "recorder_display.h"
#include "recorder_device.h"
#include "ui\videoitem\recorder_item.h"
#include "base\sleep.h"

CLRecorderDisplay::CLRecorderDisplay(CLRecorderDevice* dev, CLRecorderItem* recitem):
mDev(dev),
mRecitem(recitem)
{

}

CLRecorderDisplay::~CLRecorderDisplay()
{
	stop();
}

void CLRecorderDisplay::run()
{
	while(!needToStop())
	{
		QString text = mDev->toString();
		mRecitem->setText(text);


		//=== 2 sec delay===
		for (int i = 0; i < 20; ++i)
		{
			if (needToStop())
				break;

			CLSleep::msleep(100);
		}


	}
}
