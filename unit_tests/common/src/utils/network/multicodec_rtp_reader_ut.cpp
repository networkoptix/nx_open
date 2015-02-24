/**********************************************************
* 24 feb 2015
* a.kolesnikov
***********************************************************/

#include <iostream>
#include <iomanip>

#include <gtest/gtest.h>

#include <QtCore/QUuid>

#include <core/resource/network_resource.h>
#include <core/resource_management/resource_properties.h>
#include <utils/common/synctime.h>
#include <utils/network/multicodec_rtp_reader.h>


#if 0

TEST( QnMulticodecRtpReader, streamFetchingOverRTSP )
{
    QnResourcePropertyDictionary resPropertyDictionary;
    QnSyncTime syncTimeInstance;

    QnNetworkResourcePtr resource( new QnNetworkResource() );
    resource->setId( QUuid::createUuid().toString() );
    resource->setName( "DummyRes" );
    QAuthenticator auth;
    auth.setUser( "root" );
    auth.setPassword( "ptth" );
    resource->setAuth( auth );

    QString rtspUrl( QLatin1String("rtsp://192.168.0.27/axis-media/media.amp") );

    std::unique_ptr<QnMulticodecRtpReader> rtspStreamReader( new QnMulticodecRtpReader( resource ) );
    rtspStreamReader->setRequest( rtspUrl );
    ASSERT_TRUE( rtspStreamReader->openStream() );

    int frameCount = 0;
    QElapsedTimer t;
    t.restart();
    for( ;; )
    {
        if( t.elapsed() > 10*1000 )
        {
            std::cout<<"fps "<<std::setprecision(2)<<(frameCount / (t.elapsed() / 1000.0))<<std::endl;
            frameCount = 0;
            t.restart();
        }
        auto frame = rtspStreamReader->getNextData();
        if( !frame )
        {
            std::cerr<<"Failed to get frame from "<<rtspUrl.toStdString()<<std::endl;
            continue;
        }
        ++frameCount;
    }
}

#endif
