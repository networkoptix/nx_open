/**********************************************************
* 30 jan 2015
* a.kolesnikov
***********************************************************/

#include <gtest/gtest.h>

#include <utils/network/http/httpstreamreader.h>


TEST( HttpStreamReader, MultipleMessages )
{
    static const QByteArray messageBody1 = 
        "1xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
        "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
        "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
        "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
        "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
        "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
        "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
        "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
        "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
        "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx2";

    static const QByteArray messageBody2 = 
        "3hhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhh"
        "hhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhh"
        "hhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhh"
        "hhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhh"
        "hhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhh4";

    static const QByteArray SOURCE_DATA( 
        "HTTP/1.1 200 OK\r\n"
        "Date: Wed, 15 Nov 1995 06:25:24 GMT\r\n"
        "Last-Modified: Wed, 15 Nov 1995 04:58:08 GMT\r\n"
        "Content-Length: "+QByteArray::number(messageBody1.size())+"\r\n"
        "Content-Type: application/text\r\n"
        "\r\n"
        + messageBody1 +
        "HTTP/1.1 200 OK\r\n"
        "Date: Wed, 15 Nov 1995 06:25:24 GMT\r\n"
        "Last-Modified: Wed, 15 Nov 1995 04:58:08 GMT\r\n"
        "Content-Length: "+QByteArray::number(messageBody2.size())+"\r\n"
        "Content-Type: application/text\r\n"
        "\r\n"
        +messageBody2 );
    static const int DATA_STEP = 1;

    nx_http::HttpStreamReader streamReader;
    int messageNumber = 0;
    for( int pos = 0; pos < SOURCE_DATA.size(); pos += 1 )
    {
        size_t bytesProcessed = 0;
        ASSERT_TRUE( streamReader.parseBytes( SOURCE_DATA.mid( pos ), DATA_STEP, &bytesProcessed ) );
        ASSERT_EQ( streamReader.currentMessageNumber(), messageNumber );
        ASSERT_FALSE( streamReader.state() == nx_http::HttpStreamReader::parseError );
        if( streamReader.state() != nx_http::HttpStreamReader::messageDone )
            continue;
        ASSERT_EQ( streamReader.message().type, nx_http::MessageType::response );
        ASSERT_TRUE( streamReader.message().response != nullptr );
        ASSERT_EQ( streamReader.message().response->statusLine.statusCode, nx_http::StatusCode::ok );
        const QByteArray msgBody = streamReader.fetchMessageBody();
        if( messageNumber == 0 )
        {
            ASSERT_EQ( nx_http::getHeaderValue( streamReader.message().response->headers, "Content-Length" ).toInt(), messageBody1.size() );
            ASSERT_TRUE( msgBody == messageBody1 );
        }
        else if( messageNumber == 1 )
        {
            ASSERT_EQ( nx_http::getHeaderValue( streamReader.message().response->headers, "Content-Length" ).toInt(), messageBody2.size() );
            ASSERT_TRUE( msgBody == messageBody2 );
        }
        ++messageNumber;
    }
}

//--gtest_filter=HttpStreamReader*
