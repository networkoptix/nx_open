#include "isd_resource.h"
#include "../onvif/dataprovider/rtp264_stream_provider.h"


const char* QnPlIsdResource::MANUFACTURE = "ISD";


QnPlIsdResource::QnPlIsdResource()
{
    setAuth("root", "system");
    
}

bool QnPlIsdResource::isResourceAccessible()
{
    return updateMACAddress();
}

bool QnPlIsdResource::updateMACAddress()
{
    return true;
}

QString QnPlIsdResource::manufacture() const
{
    return MANUFACTURE;
}

void QnPlIsdResource::setIframeDistance(int /*frames*/, int /*timems*/)
{
}

QnAbstractStreamDataProvider* QnPlIsdResource::createLiveDataProvider()
{
    QString request = "stream1";


    QFile file(QLatin1String("c:/isd.txt")); // Create a file handle for the file named
    if (file.exists())
    {
        QString line;

        if (file.open(QIODevice::ReadOnly)) // Open the file
        {
            QTextStream stream(&file); // Set the stream to read from myFile
            line = stream.readLine().trimmed(); // this reads a line (QString) from the file

            if (line.length() > 0)
                request = line;
        }

    }

    return new QnRtpStreamReader(toSharedPointer(), request);
}

void QnPlIsdResource::setCropingPhysical(QRect /*croping*/)
{
}

const QnResourceAudioLayout* QnPlIsdResource::getAudioLayout(const QnAbstractMediaStreamDataProvider* dataProvider)
{
    if (isAudioEnabled()) {
        const QnRtpStreamReader* rtspReader = dynamic_cast<const QnRtpStreamReader*>(dataProvider);
        if (rtspReader && rtspReader->getDPAudioLayout())
            return rtspReader->getDPAudioLayout();
        else
            return QnPhysicalCameraResource::getAudioLayout(dataProvider);
    }
    else
        return QnPhysicalCameraResource::getAudioLayout(dataProvider);
}
