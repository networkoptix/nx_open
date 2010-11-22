#ifndef abstract_data_h_1112
#define abstract_data_h_1112

#include "../base/threadqueue.h"
#include "../base/expensiveobject.h"

struct CLAbstractData : public CLRefCounter
{
	void* dataProvider;
};

typedef CLThreadQueue<CLAbstractData*> CLDataQueue;

#endif //abstract_data_h_1112