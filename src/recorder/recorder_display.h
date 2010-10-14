#ifndef recorder_display_h_1347
#define recorder_display_h_1347

#include "base\longrunnable.h"

class CLDevice;
class CLRecorderItem;

class CLRecorderDisplay : public CLLongRunnable
{
public:
	CLRecorderDisplay(CLDevice* dev, CLRecorderItem* recitem);
	~CLRecorderDisplay();

	CLDevice* getDevice() const;
	CLRecorderItem* getRecorderItem() const;

protected:
	void run();
private:
	CLDevice* mDev;
	CLRecorderItem* mRecitem;
};



#endif //recorder_display_h_1347