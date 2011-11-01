#include "av_client_pull.h"
#include "cpul_tftp_dataprovider.h"
#include "../resource/av_resource.h"
#include "../tools/simple_tftp_client.h"
#include "../tools/AVJpegHeader.h"
#include <QMutex>
#include "core/dataprovider/media_streamdataprovider.h"

//======================================================

extern int create_sps_pps(
				   int frameWidth, 
				   int frameHeight,
				   int deblock_filter,
				   unsigned char* data, int max_datalen);

AVLastPacketSize ExtractSize(const unsigned char* arr)
{
	const int fc = 256;
	AVLastPacketSize size;
	size.x0 = static_cast<unsigned char>(arr[0]) * fc + static_cast<unsigned char>(arr[1]),
		size.y0 = static_cast<unsigned char>(arr[2]) * fc + static_cast<unsigned char>(arr[3]);
	size.width = static_cast<unsigned char>(arr[4]) * fc + static_cast<unsigned char>(arr[5]) - size.x0,
		size.height = static_cast<unsigned char>(arr[6]) * fc + static_cast<unsigned char>(arr[7]) - size.y0;
	return size;
}

//=========================================================

AVClientPullSSTFTPStreamreader::AVClientPullSSTFTPStreamreader(QnResourcePtr res):
QnPlAVClinetPullStreamReader(res),
m_black_white(false)
{
	m_timeout = 500;

	m_last_width = 0;
	m_last_height = 0;

	m_last_cam_width = 0;
	m_last_cam_height = 0;

    QnPlAreconVisionResourcePtr avRes = res.dynamicCast<QnPlAreconVisionResource>();

    m_panoramic = avRes->isPanoramic();
    m_dualsensor = avRes->isDualSensor();
    m_name = avRes->getName();


}

QnAbstractMediaDataPtr AVClientPullSSTFTPStreamreader::getNextData()
{

	QString request;
	QTextStream os(&request);

	unsigned int forecast_size = 0;
	int left;
	int top;
	int right;
	int bottom;

	int width;
	int height;

	int quality;

	bool resolutionFULL;
	int streamID;

	int bitrate;

	bool h264;

	{
			//QMutexLocker mutex(&m_mutex);

			h264 = isH264();//false;

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
				(h264 && !m_streamParam.exists("streamID")) || (h264 && !m_streamParam.exists("Bitrate")))
			{
				cl_log.log("Erorr!!! parameter is missing in stream params.", cl_logERROR);
				//return QnAbstractMediaDataPtr(0);
			}

			//=========
			left = m_streamParam.get("image_left").value();
			top = m_streamParam.get("image_top").value();
			right = m_streamParam.get("image_right").value();
			bottom = m_streamParam.get("image_bottom").value();

			if (m_dualsensor && m_black_white) //3130 || 3135
			{
				right = right/3*2;
				bottom = bottom/3*2;

				right = right/32*32;
				bottom = bottom/16*16;

				right = qMin(1280, right);
				bottom = qMin(1024, bottom);

			}

			//right = 1280;
			//bottom = 1024;

			//right/=2;
			//bottom/=2;

			width = right - left;
			height = bottom - top;

			if (m_last_cam_width==0)
				m_last_cam_width = width;

			if (m_last_cam_height==0)
				m_last_cam_height= height;

			quality = m_streamParam.get("Quality").value();
			//quality = getQuality();

			resolutionFULL = (m_streamParam.get("resolution").value() == QString("full"));

			streamID = 0;
			if (h264)
			{
				if (width!=m_last_width || height!=m_last_height || m_last_resolution!=resolutionFULL)
				{
					// if this is H.264 and if we changed image size, we need to request I frame.
					// camera itself shoud end I frame. but just in case.. to be on the save side...

					m_last_resolution = resolutionFULL;
					m_last_width = width;
					m_last_height = height;

					setNeedKeyData();
				}

				streamID = m_streamParam.get("streamID").value();
				//bitrate = m_streamParam.get("Bitrate").value.value;
				bitrate = getBitrate();
			}
			//=========

	}

	if (h264)
		quality=37-quality; // for H.264 it's not quality; it's qp 

	if (!h264)
		os <<"image";
	else
		os<<"h264";

	os<<"?res=";

	if(resolutionFULL)
		os<<"full";
	else
		os<<"half";

	os<<";x0=" << left << ";y0=" << top << ";x1=" << right << ";y1=" << bottom;

	if (!h264)
		os<<";quality=";
	else
		os<<";qp=";

	os<< quality << ";doublescan=0" << ";ssn=" << streamID;

	//h264?res=full;x0=0;y0=0;x1=1600;y1=1184;qp=27;doublescan=0;iframe=0;ssn=574;netasciiblksize1450
	request = "image?res=full;x0=0;y0=0;x1=800;y1=600;quality=10;doublescan=0;ssn=4184;";

	if (h264)
	{
		if (needKeyData())
			os <<";iframe=1;";
		else
			os <<";iframe=0;";

		//os <<"&iframe=" << *Ifarme;

		if (bitrate)
			os <<"bitrate=" << bitrate << ";";
	}
	/**/

	forecast_size = resolutionFULL ? (width*height)/4  : (width*height)/8; // 0.25 meg per megapixel as maximum 

	CLSimpleTFTPClient tftp_client( getResource().dynamicCast<QnPlAreconVisionResource>()->getHostAddress().toString().toLatin1().data(),  m_timeout, 3);

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
		expectable_header_size = create_sps_pps(m_last_cam_width , m_last_cam_height, 0, h264header, sizeof(h264header));
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
		return QnCompressedVideoDataPtr(0);
	}

	img.removeZerosAtTheEnd();

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


	if (m_dualsensor)
	{
		if (lp_size<21)
		{
			cl_log.log("last packet is too short!", cl_logERROR);
			return QnAbstractMediaDataPtr(0);
		}

		m_black_white = last_packet[20];
	}

	if (h264 && (lp_size < iframe_index))
	{
		cl_log.log("last packet is too short!", cl_logERROR);
		//delete videoData;
		return QnAbstractMediaDataPtr(0);
	}

	AVLastPacketSize size;

	if(m_dualsensor)
	{
		size = ExtractSize(last_packet + 12);
	}
	else
	{
		const unsigned char* arr = last_packet + 0x0C + 4 + 64;

		arr[0] & 4 ? resolutionFULL = true: false;
		size = ExtractSize(&arr[2]);

		if(!size.width && m_name.contains("3100"))
			size.width = 2048;

		if(!resolutionFULL)
		{
			size.width /= 2;
			size.height /= 2;
		}

	}

	m_last_cam_width = size.width;
	m_last_cam_height = size.height;

	videoData->flags |= AV_PKT_FLAG_KEY;
	if (h264)
	{

        if (last_packet[iframe_index-1] == 0)
		    videoData->flags &= ~AV_PKT_FLAG_KEY;

		//==========================================
		//put unit delimetr at the end of the frame

		char  c = 0;
		img.write(&c,1); //0
		img.write(&c,1); //0
		c = 1;
		img.write(&c,1); //1
		c = 0x09;
		img.write(&c,1); //0x09
		c = (videoData->flags&AV_PKT_FLAG_KEY) ? 0x10 : 0x30; 
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
		dst[4] = (videoData->flags&AV_PKT_FLAG_KEY) ? 0x65 : 0x41;

		img.prepareToWrite(8);
		dst = img.data() + img.size();
		dst[0] = dst[1] = dst[2] = dst[3] = dst[4] = dst[5] =  dst[6] = dst[7] = 0;

	}
	else
	{
		// writes JPEG header at very begining
		AVJpeg::Header::GetHeader((unsigned char*)img.data(), size.width, size.height, quality, m_name.toLatin1().data());
	}

	videoData->compressionType = h264 ? CODEC_ID_H264 : CODEC_ID_MJPEG;
	videoData->width = size.width;
	videoData->height = size.height;

	videoData->channelNumber = 0;

	videoData->timestamp = QDateTime::currentMSecsSinceEpoch()*1000;

	return videoData;

}

