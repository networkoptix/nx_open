#ifndef recorder_display_h_1347
#define recorder_display_h_1347

#include "base\longrunnable.h"

class CLRecorderDevice;
class CLRecorderItem;

class CLRecorderDisplay : public CLLongRunnable
{
public:
	CLRecorderDisplay(CLRecorderDevice* dev, CLRecorderItem* recitem);
	~CLRecorderDisplay();
protected:
	void run();
private:
	CLRecorderDevice* mDev;
	CLRecorderItem* mRecitem;
};



#endif //recorder_display_h_1347