#ifndef abstract_data_h_1112
#define abstract_data_h_1112
#include "common/threadqueue.h"
#include "common/expensiveobject.h"


class CLStreamreader;

struct CLAbstractData : public CLRefCounter
{
	CLStreamreader* dataProvider;
};

typedef CLThreadQueue<CLAbstractData*> CLDataQueue;

#endif //abstract_data_h_1112
