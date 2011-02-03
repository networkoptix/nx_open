#include "cpul_httpstremreader.h"
#include "../../../base/log.h"
#include "../../../network/simple_http_client.h"
#include "../../../data/mediadata.h"
#include "../devices/av_device.h"


AVClientPullSSHTTPStreamreader::AVClientPullSSHTTPStreamreader(CLDevice* dev):
CLAVClinetPullStreamReader(dev)
{

	CLAreconVisionDevice* device = static_cast<CLAreconVisionDevice*>(dev);

	m_port = 69;
	m_timeout = 500;
	m_auth = device->getAuth();
	m_model = device->getModel();

}


CLAbstractMediaData* AVClientPullSSHTTPStreamreader::getNextData()
{

	QString request;
	QTextStream os(&request);

	unsigned int forecast_size = 0;
	int left;
	int top;
	int right;
	int bottom;

	int width, height;


	int quality;
	int bitrate;

	bool resolutionFULL;
	int streamID;

	bool h264;



	{
			QMutexLocker mutex(&m_params_CS);

			h264 = isH264();

			/*
			if (m_streamParam.exists("Codec")) // cam is not jpeg only
			{
				CLParam codec = m_streamParam.get("Codec");
				if (codec.value.value != QString("JPEG"))
					h264 = true;
			}
			/**/
			
			if (!m_streamParam.exists("Quality") || !m_streamParam.exists("resolution") || 
				!m_streamParam.exists("image_left") || !m_streamParam.exists("image_top") ||
				!m_streamParam.exists("image_right") || !m_streamParam.exists("image_bottom") ||
				(h264 && !m_streamParam.exists("streamID")) )
			{
				cl_log.log("Erorr!!! parameter is missing in stream params.", cl_logERROR);
				return 0;
			}

			//=========
			left = m_streamParam.get("image_left").value.value;
			top = m_streamParam.get("image_top").value.value;
			right = m_streamParam.get("image_right").value.value;
			bottom = m_streamParam.get("image_bottom").value.value;

			width = right - left;
			height = bottom - top;


			//quality = m_streamParam.get("Quality").value.value;
			quality = getQuality();

			resolutionFULL = (m_streamParam.get("resolution").value.value == QString("full"));
			streamID = 0;
			if (h264)
			{
				streamID = m_streamParam.get("streamID").value.value;
				//bitrate = m_streamParam.get("Bitrate").value.value;
				bitrate = getBitrate();
			}
			//=========


			if (h264)
				quality=37-quality; // for H.264 it's not quality; it's qp 

			
			if (!h264)
				os <<"image";
			else
				os<<"h264f";

			os<<"?res=";

			if(resolutionFULL)
				os<<"full";
			else
				os<<"half";

			os<<"&x0=" << left << "&y0=" << top << "&x1=" << right << "&y1=" << bottom;

			if (!h264)
				os<<"&quality=";
			else
				os<<"&qp=";

			os<< quality << "&doublescan=1" << "&ssn=" << streamID;

			
			if (h264)
			{
				//os <<"&iframe=" << *Ifarme;
				if (needKeyData())
					os <<"&iframe=1";


				if (bitrate)
					os <<"&bitrate=" << bitrate;
			}
	

			forecast_size = resolutionFULL ? (width*height)/2  : (width*height)/4; // 0.5 meg per megapixel; to avoid mem realock
	}


	
	
	CLSimpleHTTPClient http_client((static_cast<CLAreconVisionDevice*>(m_device))->getIP(), m_port, m_timeout, m_auth);

	http_client.setRequestLine(request);
	http_client.openStream();

	CLCompressedVideoData* videoData = 0;

	if (!http_client.isOpened())
		return 0;

	videoData = new CLCompressedVideoData(CL_MEDIA_ALIGNMENT,forecast_size);
	CLByteArray& img = videoData->data;


	while(http_client.isOpened())
	{
		int readed = http_client.read(img.prepareToWrite(2000), 2000);

		if (readed<0) // error
		{
			//delete videoData;
			videoData->releaseRef();
			return 0;
		}

		img.done(readed);

		if (img.size()>CL_MAX_DATASIZE)
		{
			cl_log.log("Image is too big!!", cl_logERROR);
			//delete videoData;
			videoData->releaseRef();
			return 0;
		}
	}

	img.removeZerrowsAtTheEnd();

	
	//unit delimetr
	if (h264)
	{

		char  c = 0;
		img.write(&c,1); //0
		img.write(&c,1); //0
		c = 1;
		img.write(&c,1); //1
		c = 0x09;
		img.write(&c,1); //0x09
		c = 0x10; // 0x10
		img.write(&c,1); //0x09

	}

	//video/H.264I
	videoData->compressionType = h264 ? CL_H264 : CL_JPEG;
	videoData->width = resolutionFULL ? width : width>>1;
	videoData->height = resolutionFULL ? height : height>>1;

	videoData->keyFrame = true;

	if (h264) // for jpej keyFrame always true
	{
		if (!http_client.exist("Content-Type")) // very strange 
		{
			videoData->keyFrame = false;
		}
		else
			if (http_client.get("Content-Type")==QString("video/H.264P"))
				videoData->keyFrame = false;
		
	}

	videoData->channel_num = 0;

	videoData->timestamp = QDateTime::currentMSecsSinceEpoch()*1000;

	return videoData;

}

