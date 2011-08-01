#include "av_client_pull.h"
#include "resource/resource.h"
#include "panoramic_cpul_tftp_dataprovider.h"
#include "../resource/av_resource.h"
#include "../tools/simple_tftp_client.h"
#include "common/bytearray.h"
#include "datapacket/mediadatapacket.h"
#include "common/base.h"
#include "../tools/AVJpegHeader.h"

//======================================================

extern int create_sps_pps(
				   int frameWidth, 
				   int frameHeight,
				   int deblock_filter,
				   unsigned char* data, int max_datalen);

extern AVLastPacketSize ExtractSize(const unsigned char* arr);

//=========================================================

AVPanoramicClientPullSSTFTPStreamreader ::AVPanoramicClientPullSSTFTPStreamreader(QnResourcePtr res):
QnPlAVClinetPullStreamReader(res)
{

	m_timeout = 500;
	m_last_width = 1600;
	m_last_height = 1200;

    QnPlAreconVisionResourcePtr avRes = res.dynamicCast<QnPlAreconVisionResource>();

    m_panoramic = avRes->isPanoramic();
    m_dualsensor = avRes->isDualSensor();
    m_name = avRes->getName();


}

QnAbstractDataPacketPtr AVPanoramicClientPullSSTFTPStreamreader::getNextData()
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
		QMutexLocker mutex(&m_mtx);

		h264 = isH264();;

		if (h264) // cam is not jpeg only
		{

			if (!m_streamParam.exists("streamID"))
			{
				cl_log.log("Erorr!!! parameter is missing in stream params.", cl_logERROR);
				return QnAbstractMediaDataPacketPtr(0);
			}

			streamID = m_streamParam.get("streamID").value;

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

	CLSimpleTFTPClient tftp_client(getResource().dynamicCast<QnPlAreconVisionResource>()->getHostAddress().toString().toLatin1().data(),  m_timeout, 3);

	QnCompressedVideoDataPtr videoData ( new QnCompressedVideoData(CL_MEDIA_ALIGNMENT,forecast_size) );
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
		return QnAbstractMediaDataPacketPtr(0);
	}

	img.removeZerrowsAtTheEnd();

	int lp_size;
	const unsigned char* last_packet = tftp_client.getLastPacket(lp_size);

	int iframe_index;

    if (m_panoramic)
    {
        iframe_index = 89;
    }
    else if (m_dualsensor)
    {
        iframe_index = 98;
    }
    else
        iframe_index = 93;


	AVLastPacketSize size;

	if(m_panoramic)
	{
		const unsigned char* arr = last_packet + 0x0C;
		arr[0] & 4 ? resolutionFULL = true : false;
		size = ExtractSize(&arr[2]);

		if (size.width>5000 || size.width<0 || size.height>5000 || size.height<0) // bug of arecontvision firmware 
		{
			return QnAbstractMediaDataPacketPtr(0);
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
		cl_log.log("last packet is too short!", cl_logERROR);
		return QnAbstractMediaDataPacketPtr(0);
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

				cl_log.log("moving data!!", cl_logWARNING);

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
		/**/

		// we also need to put very begining of SH
		dst[0] = dst[1] = dst[2] = 0; dst[3] = 1;
		dst[4] = videoData->keyFrame ? 0x65 : 0x41;

		img.prepareToWrite(8);
		dst = img.data() + img.size();
		dst[0] = dst[1] = dst[2] = dst[3] = dst[4] = dst[5] =  dst[6] = dst[7] = 0;

	}
	else
	{

		// writes JPEG header at very beginning
		AVJpeg::Header::GetHeader((unsigned char*)img.data(), size.width, size.height, quality, m_name.toLatin1().data());
	}

	videoData->compressionType = h264 ? CODEC_ID_H264 : CODEC_ID_MJPEG;
	videoData->width = size.width;
	videoData->height = size.height;

	videoData->timestamp = QDateTime::currentMSecsSinceEpoch()*1000;

	return videoData;

}

bool AVPanoramicClientPullSSTFTPStreamreader::needKeyData() const
{
	for (int i = 0; i < m_NumaberOfVideoChannels; ++i)
		if (m_gotKeyFrame[i]<2)  // due to bug of AV panoramic H.264 cam. cam do not send frame with diff resolution of resolution changed. first I frame comes with old resolution
			return true;

	return false;

}
