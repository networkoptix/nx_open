#ifndef cl_av_clien_pull1529
#define cl_av_clien_pull1529

#include "../../../streamreader/cpull_stremreader.h"

class CLAVClinetPullStreamReader : public CLClientPullStreamreader
{
public:
	CLAVClinetPullStreamReader(CLDevice* dev );
	virtual ~CLAVClinetPullStreamReader();

	virtual void setQuality(StreamQuality q);

};

#endif //cl_av_clien_pull1529
 
