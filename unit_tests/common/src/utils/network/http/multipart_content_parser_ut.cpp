/**********************************************************
* 8 sep 2014
* a.kolesnikov
***********************************************************/

#include <deque>

#include <gtest/gtest.h>

#include <utils/media/custom_output_stream.h>
#include <utils/network/http/multipart_content_parser.h>


TEST( HttpMultipartContentParser, genericTest )
{
    //const nx::Buffer frame1 = 
    //    "1xxxxxxxxxxxxxxx\rxxxxxxxx\nxxxxxxxxxx"
    //    "xxxxxxxxxxxxxxxxxxxxxxxxxxx\r\nxxxxxx2";
    const nx::Buffer frame1 = 
        "1xxxxxxxxxxxxxxx\rxxxxxxxx\nxxxxxxxxxx"
        "xxxxxxxxxxxxxxxxx\r\nxxxxxxxxxxxxxxxx2";
    const nx::Buffer frame2 = 
        "3xxxxxxxxxxxxx\nxxxxxxxxxxxxxxxxxxxx"
        "xxxxxxxxxxxxxxx\r\r\rx\rxxxxxxxxxxxxxxxxxx"
        "xxxxxxxxxxxxxxxxxxxxx\n\n\n\nxxx\r\n\r\nxxxxxxxxx4";
    const nx::Buffer frame3 = "";
    //const nx::Buffer frame4 = 
    //    "5xxxxxxxxxx\r\n\r\n\r\n\r\n\r\r\r\r\r\n\n\n\n6";
    const nx::Buffer frame4 = 
        "5xxxxxxxxxx\r\n\r\n\r\n\r\n\r\r\r\r\r\n\n\n\nyyyyyyyyyyyyyy6";

    const nx::Buffer testData = 
        "--fbdr\r\n"
        "Content-Type: image/jpeg\r\n"
        "\r\n"
        +frame1+
        "\r\n"
        "--fbdr\r\n"
        "Content-Type: image/jpeg\r\n"
        "\r\n"
        +frame2+
        "\r\n"
        "--fbdr\r\n"
        "Content-Length: "+nx::Buffer::number(frame3.size())+"\r\n"
        "Content-Type: image/jpeg\r\n"
        "\r\n"
        +frame3+
        "\r\n"
        "--fbdr\r\n"
        "Content-Type: image/jpeg\r\n"
        "\r\n"
        +frame4+
        "\r\n";

    for( int dataStep = 1; dataStep < testData.size(); ++dataStep )
    {
        nx_http::MultipartContentParser parser;
        parser.setContentType( "multipart/x-mixed-replace;boundary=fbdr" );

        std::deque<QByteArray> frames;
        auto decodedFramesProcessor = [&frames]( const QnByteArrayConstRef& data ) {
            frames.push_back( data );
        };

        parser.setNextFilter(
            std::make_shared<CustomOutputStream<decltype(decodedFramesProcessor)> >(
                decodedFramesProcessor) );

#if 0   //TODO #ak fix
        for( size_t pos = 0; pos < testData.size(); pos += dataStep )
            parser.processData( QnByteArrayConstRef(
                testData,
                pos,
                pos+dataStep <= testData.size() ? dataStep : QnByteArrayConstRef::npos ) );
#else
        parser.processData( testData );
#endif
        parser.flush();

        ASSERT_EQ( frames.size(), 4 );
        ASSERT_EQ( frames[0], frame1 );
        ASSERT_EQ( frames[1], frame2 );
        ASSERT_EQ( frames[2], frame3 );
        ASSERT_EQ( frames[3], frame4 );
    }
}

TEST( HttpMultipartContentParser, onlySizedData )
{
    //const nx::Buffer frame1 = 
    //    "1xxxxxxxxxxxxxxx\rxxxxxxxx\nxxxxxxxxxx"
    //    "xxxxxxxxxxxxxxxxxxxxxxxxxxx\r\nxxxxxx2";
    const nx::Buffer frame1 = 
        "1xxxxxxxxxxxxxxx\rxxxxxxxx\nxxxxxxxxxx"
        "xxxxxxxxxxxxxxxxx\r\nxxxxxxxxxxxxxxxx2";
    const nx::Buffer frame2 = 
        "3xxxxxxxxxxxxx\nxxxxxxxxxxxxxxxxxxxx"
        "xxxxxxxxxxxxxxx\r\r\rx\rxxxxxxxxxxxxxxxxxx"
        "xxxxxxxxxxxxxxxxxxxxx\n\n\n\nxxx\r\n\r\nxxxxxxxxx4";
    const nx::Buffer frame3 = "";
    //const nx::Buffer frame4 = 
    //    "5xxxxxxxxxx\r\n\r\n\r\n\r\n\r\r\r\r\r\n\n\n\n6";
    const nx::Buffer frame4 = 
        "5xxxxxxxxxx\r\n\r\n\r\n\r\n\r\r\r\r\r\n\n\n\nyyyyyyyyyyyyyy6";

    const nx::Buffer testData = 
        "--fbdr\r\n"
        "Content-Length: "+nx::Buffer::number(frame1.size())+"\r\n"
        "Content-Type: image/jpeg\r\n"
        "\r\n"
        +frame1+
        "\r\n"
        "--fbdr\r\n"
        "Content-Length: "+nx::Buffer::number(frame2.size())+"\r\n"
        "Content-Type: image/jpeg\r\n"
        "\r\n"
        +frame2+
        "\r\n"
        "--fbdr\r\n"
        "Content-Length: "+nx::Buffer::number(frame3.size())+"\r\n"
        "Content-Type: image/jpeg\r\n"
        "\r\n"
        +frame3+
        "\r\n"
        "--fbdr\r\n"
        "Content-Length: "+nx::Buffer::number(frame4.size())+"\r\n"
        "Content-Type: image/jpeg\r\n"
        "\r\n"
        +frame4+
        "\r\n";

    for( int dataStep = 1; dataStep < testData.size(); ++dataStep )
    {
        nx_http::MultipartContentParser parser;
        parser.setContentType( "multipart/x-mixed-replace;boundary=fbdr" );

        std::deque<QByteArray> frames;
        auto decodedFramesProcessor = [&frames]( const QnByteArrayConstRef& data ) {
            frames.push_back( data );
        };

        parser.setNextFilter(
            std::make_shared<CustomOutputStream<decltype(decodedFramesProcessor)> >(
                decodedFramesProcessor) );

        for( int pos = 0; pos < testData.size(); pos += dataStep )
            parser.processData( QnByteArrayConstRef(
                testData,
                pos,
                pos+dataStep <= testData.size() ? dataStep : QnByteArrayConstRef::npos ) );
        parser.flush();

        ASSERT_EQ( frames.size(), 4 );
        ASSERT_EQ( frames[0], frame1 );
        ASSERT_EQ( frames[1], frame2 );
        ASSERT_EQ( frames[2], frame3 );
        ASSERT_EQ( frames[3], frame4 );
    }

    //TODO #ak test with Content-Length specified
}
