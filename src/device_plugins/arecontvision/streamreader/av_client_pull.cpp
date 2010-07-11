#include "av_client_pull.h"

CLAVClinetPullStreamReader::CLAVClinetPullStreamReader(CLDevice* dev ):
CLClientPullStreamreader(dev)
{

}


CLAVClinetPullStreamReader::~CLAVClinetPullStreamReader()
{

}


void CLAVClinetPullStreamReader::setQuality(StreamQuality q)
{

	CLClientPullStreamreader::setQuality(q);


	CLParamList pl = getStreamParam();

	switch(q)
	{
	case CLSHighest:
	case CLSHigh:

		pl.get("resolution").value.value = "full";

		pl.get("quality").value.value = "16";

		break;


	case CLSNormal:

		pl.get("resolution").value.value = "half";
		pl.get("quality").value.value = "10";

	    break;


	case CLSLow:
	case CLSLowest:

		pl.get("resolution").value.value = "half";
		pl.get("quality").value.value = "2";

	    break;
	}


	setStreamParams(pl);


}