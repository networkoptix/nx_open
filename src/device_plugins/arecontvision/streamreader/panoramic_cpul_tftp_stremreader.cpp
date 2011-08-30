#include "panoramic_cpul_tftp_stremreader.h"
#include "../../../base/log.h"
#include "../tools/simple_tftp_client.h"
#include "../tools/AVJpegHeader.h"
#include "../devices/av_device.h"

//======================================================

extern int create_sps_pps(
				   int frameWidth,
				   int frameHeight,
				   int deblock_filter,
				   unsigned char* data, int max_datalen);

extern AVLastPacketSize ExtractSize(const unsigned char* arr);

//=========================================================

AVPanoramicClientPullSSTFTPStreamreader ::AVPanoramicClientPullSSTFTPStreamreader  (CLDevice* dev ):
CLAVClinetPullStreamReader(dev)
{
	CLAreconVisionDevice* device = static_cast<CLAreconVisionDevice*>(dev);

	m_model = device->getModel();

	m_timeout = 500;

	m_last_width = 1600;
	m_last_height = 1200;

}

CLAbstractMediaData* AVPanoramicClientPullSSTFTPStreamreader::getNextData()
{

	QString request;
	QTextStream os(&request);

	unsigned int forecast_size = 0;

	bool h264;
	int streamID = 0;

	int width = m_last_width;
	int height = m_last_height;
	bool resolutionFULL = true;

	int quality = 15;

	{
		QMutexLocker mutex(&m_params_CS);

		h264 = isH264();;

		if (h264) // cam is not jpeg only
		{

			if (!m_streamParam.exists(QLatin1String("streamID")))
			{
				cl_log.log(QLatin1String("Erorr!!! parameter is missing in stream params."), cl_logERROR);
				return 0;
			}

			streamID = m_streamParam.get(QLatin1String("streamID")).value.value;

		}

	}

	if (!h264)
		os <<"image";
	else
	{
		os<<"h264?ssn="<< streamID;
	}

	if (h264)
	{
		if (needKeyData())
			os <<";iframe=1;";
		else
			os <<";iframe=0;";

	}

	forecast_size = (width*height)/2; // 0.5 meg per megapixel as maximum

	CLSimpleTFTPClient tftp_client((static_cast<CLAreconVisionDevice*>(m_device))->getIP().toString().toLatin1().data(),  m_timeout, 3);

	CLCompressedVideoData* videoData = new CLCompressedVideoData(CL_MEDIA_ALIGNMENT,forecast_size);
	CLByteArray& img = videoData->data;

	//==========================================
	int expectable_header_size;
	if (h264)
	{
		// 0) in tftp mode cam do not send image header, we need to form it.
		// 1) h264 header has variable len; depends on actual width and hight;
		// 2) this not very likely, but still possible( for example if camera automatically switched from night to day mode and change resolution): returned resolution is not equal to requested resolution

		// 3) we do not nees to put SPS and PPS in before each, but we can.

		unsigned char h264header[50];
		expectable_header_size = create_sps_pps(resolutionFULL ? width : width/2, resolutionFULL ? height: height/2, 0, h264header, sizeof(h264header));
		img.prepareToWrite(expectable_header_size + 5 ); // 5: from cam in tftp mode we receive data starts from second byte of slice header; so we need to put start code and first byte
		img.done(expectable_header_size + 5 );

		// please note that we did not write a single byte to img, we just move position...
		// we will write header based on last packet information
	}
	else
	{
		// in jpeg image header size has constant len
		img.prepareToWrite(AVJpeg::Header::GetHeaderSize()-2);
		img.done(AVJpeg::Header::GetHeaderSize()-2); //  for some reason first 2 bytes from cam is trash

		// please note that we did not write a single byte to img, we just move position...
		// we will write header based on last packet information
	}

	//==========================================

	int readed = tftp_client.read(request.toLatin1().data(), img);

	if (readed == 0) // cannot read data
	{
		//delete videoData;
		videoData->releaseRef();
		return 0;
	}

	img.removeZerosAtTheEnd();

	int lp_size;
	const unsigned char* last_packet = tftp_client.getLastPacket(lp_size);

	int iframe_index;

	switch(m_model)
	{
	case AV8365:
	case AV8185:
		iframe_index = 89;
		break;
	case AV3135:
		iframe_index = 88;
		break;
	default:
		iframe_index = 93;
	}

	AVLastPacketSize size;

	if(AV8365 == m_model || AV8185 == m_model || AV8180 == m_model || AV8360 == m_model)
	{
		const unsigned char* arr = last_packet + 0x0C;
		arr[0] & 4 ? resolutionFULL = true : false;
		size = ExtractSize(&arr[2]);

		if (size.width>5000 || size.width<0 || size.height>5000 || size.height<0) // bug of arecontvision firmware
		{
			videoData->releaseRef();
			return 0;
		}

		m_last_width = size.width;
		m_last_height = size.height;

		videoData->channelNumber = arr[0] & 3;

		//multisensor_is_zoomed = arr[0] & 8;
		//IMAGE_RESOLUTION res;
		//arr[0] & 4 ? res = imFULL : res = imHALF;

		quality = arr[1];
	}

	if (h264 && (lp_size < iframe_index))
	{
		cl_log.log(QLatin1String("last packet is too short!"), cl_logERROR);
		//delete videoData;
		videoData->releaseRef();
		return 0;
	}

	videoData->keyFrame = true;
	if (h264)
	{
		videoData->keyFrame = last_packet[iframe_index-1];
		//==========================================
		//put unit delimetr at the end of the frame

		char  c = 0;
		img.write(&c,1); //0
		img.write(&c,1); //0
		c = 1;
		img.write(&c,1); //1
		c = 0x09;
		img.write(&c,1); //0x09
		c = videoData->keyFrame ? 0x10 : 0x30;
		img.write(&c,1); // 0x10

		//==========================================
		char* dst = img.data();
		//if (videoData->keyFrame) // only if I frame we need SPS&PPS
		{
			unsigned char h264header[50];
			int header_size = create_sps_pps(size.width, size.height, 0, h264header, sizeof(h264header));

			if (header_size!=expectable_header_size) // this should be very rarely
			{
				int diff = header_size - expectable_header_size;
				if (diff>0)
					img.prepareToWrite(diff);

				cl_log.log(QLatin1String("moving data!!"), cl_logWARNING);

				memmove(img.data() + 5 + header_size, img.data() + 5 + expectable_header_size, img.size() - (5 + expectable_header_size));
				img.done(diff);
			}

			dst = img.data();
			memcpy(dst, h264header, header_size);
			dst+= header_size;
		}
		/*
		else
		{
			img.ignore_first_bytes(expectable_header_size); // if you decoder needs compressed data alignment, just do not do it. ffmpeg will delay one frame if do not do it.
			dst+=expectable_header_size;
		}
		*/

		// we also need to put very begining of SH
		dst[0] = dst[1] = dst[2] = 0; dst[3] = 1;
		dst[4] = videoData->keyFrame ? 0x65 : 0x41;

		img.prepareToWrite(8);
		dst = img.data() + img.size();
		dst[0] = dst[1] = dst[2] = dst[3] = dst[4] = dst[5] =  dst[6] = dst[7] = 0;

	}
	else
	{
		QString model;
		model.setNum(m_model);

		// writes JPEG header at very begining
		AVJpeg::Header::GetHeader((unsigned char*)img.data(), size.width, size.height, quality,model.toLatin1().data());
	}

	videoData->compressionType = h264 ? CODEC_ID_H264 : CODEC_ID_MJPEG;
	videoData->width = size.width;
	videoData->height = size.height;

	videoData->timestamp = QDateTime::currentMSecsSinceEpoch()*1000;

	return videoData;

}

bool AVPanoramicClientPullSSTFTPStreamreader::needKeyData() const
{
	for (int i = 0; i < m_channel_number; ++i)
		if (m_gotKeyFrame[i]<2)  // due to bug of AV panoramic H.264 cam. cam do not send frame with diff resolution of resolution changed. first I frame comes with old resolution
			return true;

	return false;

}
