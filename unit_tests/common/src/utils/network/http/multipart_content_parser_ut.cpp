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
    nx_http::MultipartContentParser parser;
    parser.setContentType( "multipart/x-mixed-replace;boundary=fbdr" );

    std::deque<QByteArray> frames;
    auto decodedFramesProcessor = [&frames]( const QnByteArrayConstRef& data ) {
        frames.push_back( data );
    };

    parser.setNextFilter(
        std::make_shared<CustomOutputStream<decltype(decodedFramesProcessor)> >(
            decodedFramesProcessor) );

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
    //const nx::Buffer frame3 = 
    //    "5xxxxxxxxxx\r\n\r\n\r\n\r\n\r\r\r\r\r\n\n\n\n6";
    const nx::Buffer frame3 = 
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
        "Content-Type: image/jpeg\r\n"
        "\r\n"
        +frame3+
        "\r\n";
    parser.processData( testData );
    parser.flush();

    ASSERT_EQ( frames.size(), 3 );
    ASSERT_EQ( frames[0], frame1 );
    ASSERT_EQ( frames[1], frame2 );
    ASSERT_EQ( frames[2], frame3 );

    //TODO #ak test with Content-Length specified
}
