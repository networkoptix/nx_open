#ifndef av_singlesensor_h_1252
#define av_singlesensor_h_1252


#include "av_device.h"
#include "av_http_client.h"



class CLAreconPanoramicDevice : public CLAreconVisionDevice
{
public:

	bool getDescription()
	{
		return true;
	};



};

#endif // av_singlesensor_h_1252