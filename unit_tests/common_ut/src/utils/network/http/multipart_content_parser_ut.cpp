/**********************************************************
* 8 sep 2014
* a.kolesnikov
***********************************************************/

#include <deque>
#include <random>

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
        "\r\n--fbdr"
        "\r\n"
        "Content-Type: image/jpeg\r\n"
        "\r\n"
        +frame1+
        "\r\n--fbdr"
        "\r\n"
        "Content-Type: image/jpeg\r\n"
        "\r\n"
        +frame2+
        "\r\n--fbdr"
        "\r\n"
        "Content-Length: "+nx::Buffer::number(frame3.size())+"\r\n"
        "Content-Type: image/jpeg\r\n"
        "\r\n"
        +frame3+
        "\r\n--fbdr"
        "\r\n"
        "Content-Type: image/jpeg\r\n"
        "\r\n"
        +frame4;

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
        "\r\n--fbdr"
        "\r\n"
        "Content-Length: "+nx::Buffer::number(frame1.size())+"\r\n"
        "Content-Type: image/jpeg\r\n"
        "\r\n"
        +frame1+
        "\r\n--fbdr"
        "\r\n"
        "Content-Length: "+nx::Buffer::number(frame2.size())+"\r\n"
        "Content-Type: image/jpeg\r\n"
        "\r\n"
        +frame2+
        "\r\n--fbdr"
        "\r\n"
        "Content-Length: "+nx::Buffer::number(frame3.size())+"\r\n"
        "Content-Type: image/jpeg\r\n"
        "\r\n"
        +frame3+
        "\r\n--fbdr"
        "\r\n"
        "Content-Length: "+nx::Buffer::number(frame4.size())+"\r\n"
        "Content-Type: image/jpeg\r\n"
        "\r\n"
        +frame4;

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

TEST( HttpMultipartContentParser, unSizedDataSimple )
{
    const nx::Buffer frame1 = 
        "a\rb";
    const nx::Buffer frame2 =
        "c\rd";

    const nx::Buffer testData =
        "\r\n--fbdr"    //boundary
        "\r\n"
        "Content-Type: image/jpeg\r\n"
        "\r\n"
        + frame1 +
        "\r\n--fbdr"
        "\r\n"
        "Content-Type: image/jpeg\r\n"
        "\r\n"
        + frame2/* +
        "\r\n"*/;

    //for( int dataStep = 1; dataStep < testData.size(); ++dataStep )
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
                decodedFramesProcessor ) );

        for( size_t pos = 0; pos < testData.size(); pos += dataStep )
            parser.processData( QnByteArrayConstRef(
                testData,
                pos,
                pos + dataStep <= testData.size() ? dataStep : QnByteArrayConstRef::npos ) );
        parser.flush();

        ASSERT_EQ( frames.size(), 2 );
        ASSERT_EQ( frames[0], frame1 );
        ASSERT_EQ( frames[1], frame2 );
    }
}

TEST( HttpMultipartContentParser, unSizedData )
{
    static const size_t FRAMES_COUNT = 10;
    static const size_t FRAME_SIZE_MIN = 4096;
    static const size_t FRAME_SIZE_MAX = 1024*1024;

    //static const size_t FRAMES_COUNT = 2000;
    //static const size_t FRAME_SIZE_MIN = 10;
    //static const size_t FRAME_SIZE_MAX = 10000;

    //const size_t DATA_STEPS[] = { 1 };
    const size_t DATA_STEPS[] = { 1, 2, 3, 20, 48, 4*1024, 64*1024 };

    //for( int j = 0; j < 2000; ++j )
    {
        //generating test frames
        std::vector<nx::Buffer> testFrames( FRAMES_COUNT );
        std::random_device rd;
        std::uniform_int_distribution<size_t> frameSizeDistr( FRAME_SIZE_MIN, FRAME_SIZE_MAX );
        std::uniform_int_distribution<char> frameContentDistr;
        for( nx::Buffer& testFrame : testFrames )
        {
            testFrame.resize( frameSizeDistr(rd) );
            std::generate(
                testFrame.data(),
                testFrame.data()+ testFrame.size(),
                [&]{ return frameContentDistr(rd); } );
            ASSERT_EQ( testFrame.indexOf( "--fbdr" ), -1 );
        }

        //test data
        nx::Buffer testData;
        for( const nx::Buffer& testFrame: testFrames )
        {
            testData += 
                "\r\n--fbdr"    //delimiter
                "\r\n"
                "Content-Type: image/jpeg\r\n"
                "\r\n"
                + testFrame;
        }

        for( const auto dataStep: DATA_STEPS )
        {
            nx_http::MultipartContentParser parser;
            parser.setContentType( "multipart/x-mixed-replace;boundary=fbdr" );

            std::deque<QByteArray> readFrames;
            auto decodedFramesProcessor = [&readFrames]( const QnByteArrayConstRef& data ) {
                readFrames.push_back( data );
            };

            parser.setNextFilter(
                std::make_shared<CustomOutputStream<decltype(decodedFramesProcessor)> >(
                    decodedFramesProcessor ) );

            for( int pos = 0; pos < testData.size(); pos += dataStep )
                parser.processData( QnByteArrayConstRef(
                    testData,
                    pos,
                    pos + dataStep <= testData.size() ? dataStep : QnByteArrayConstRef::npos ) );
            parser.flush();

            ASSERT_EQ( readFrames.size(), testFrames.size() );
            for( size_t i = 0; i < readFrames.size(); ++i )
            {
                ASSERT_EQ( readFrames[i], testFrames[i] );
            }
        }
    }
}
