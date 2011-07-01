#ifndef cl_avigilon_clien_pul1721
#define cl_avigilon_clien_pul1721

#include "common\streamreader\cpull_stremreader.h"



class CLAvigilonStreamreader: public CLClientPullStreamreader
{
public:
	CLAvigilonStreamreader(CLDevice* dev);
	virtual ~CLAvigilonStreamreader();

    virtual CLAbstractMediaData* getNextData();
};

#endif //cl_av_clien_pull1529
