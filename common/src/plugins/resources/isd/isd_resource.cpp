#include "isd_resource.h"
#include "../onvif/dataprovider/rtp264_stream_provider.h"


extern const char* ISD_MANUFACTURER;
const char* QnPlIsdResource::MANUFACTURE = ISD_MANUFACTURER;


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

    return new RTP264StreamReader(toSharedPointer(), request);
}

void QnPlIsdResource::setCropingPhysical(QRect /*croping*/)
{
}
