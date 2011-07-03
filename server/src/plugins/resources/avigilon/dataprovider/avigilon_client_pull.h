#ifndef cl_avigilon_clien_pul1721
#define cl_avigilon_clien_pul1721

#include "dataprovider/cpull_streamdataprovider.h"



class CLAvigilonStreamreader: public CLClientPullStreamreader
{
public:
	CLAvigilonStreamreader(QnResource* dev);
	virtual ~CLAvigilonStreamreader();

    virtual QnAbstractMediaDataPacketPtr getNextData();
};

#endif //cl_av_clien_pull1529
