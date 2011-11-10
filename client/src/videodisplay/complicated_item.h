#ifndef colplicated_item_h_1706
#define colplicated_item_h_1706

#include "core/resource/resource.h"



class CLAbstractSceneItem;

class CLAbstractComplicatedItem
{
public:
	virtual ~CLAbstractComplicatedItem(){};

	virtual QnResourcePtr getDevice() const = 0;

	virtual void startDispay() = 0;
	virtual void beforestopDispay() = 0;
	virtual void stopDispay() = 0;

	virtual CLAbstractSceneItem* getSceneItem() const = 0;

};

#endif //colplicated_item_h_1706
