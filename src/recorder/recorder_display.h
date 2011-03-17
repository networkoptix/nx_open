#ifndef recorder_display_h_1347
#define recorder_display_h_1347

#include "base/longrunnable.h"
#include "videodisplay/complicated_item.h"

class CLDevice;
class CLRecorderItem;

class CLRecorderDisplay : public CLLongRunnable, public CLAbstractComplicatedItem
{
public:
	CLRecorderDisplay(CLDevice* dev, CLRecorderItem* recitem);
	~CLRecorderDisplay();

	CLDevice* getDevice() const;

	virtual void startDispay();
	virtual void beforestopDispay();
	virtual void stopDispay();

	virtual CLAbstractSceneItem* getSceneItem() const;

	CLRecorderItem* getRecorderItem() const;

protected:
	void run();
private:
	CLDevice* mDev;
	CLRecorderItem* mRecitem;
};

#endif //recorder_display_h_1347
