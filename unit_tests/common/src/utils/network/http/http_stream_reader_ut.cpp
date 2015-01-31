/**********************************************************
* 30 jan 2015
* a.kolesnikov
***********************************************************/

#include <gtest/gtest.h>

#include <utils/network/http/httpstreamreader.h>


class TestMessageData
{
public:
    int statusCode;
    nx::Buffer headers;
    nx::Buffer msgBody;

    TestMessageData(
        int _statusCode,
        nx::Buffer _headers,
        nx::Buffer _msgBody )
    :
        statusCode( _statusCode ),
        headers( _headers ),
        msgBody( _msgBody )
    {
    }
};

TEST( HttpStreamReader, MultipleMessages )
{
    //vector<pair<message, message body>>
    std::vector<TestMessageData> messagesToParse;

    messagesToParse.push_back( TestMessageData(
        200,
        "Date: Wed, 15 Nov 1995 06:25:24 GMT\r\n"
        "Last-Modified: Wed, 15 Nov 1995 04:58:08 GMT\r\n"
        "Content-Type: application/text\r\n",
        
        "1xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
        "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
        "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
        "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
        "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
        "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
        "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
        "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
        "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
        "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx2"
        ) );

    messagesToParse.push_back( TestMessageData(
        200,
        "Date: Wed, 15 Nov 1995 06:25:24 GMT\r\n"
        "Last-Modified: Wed, 15 Nov 1995 04:58:08 GMT\r\n"
        "Content-Type: application/text\r\n",
        
        "3hhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhh"
        "hhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhh"
        "hhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhh"
        "hhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhh"
        "hhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhh4"
        ) );

    messagesToParse.push_back( TestMessageData(
        451,
        "Date: Wed, 15 Nov 1995 06:25:24 GMT\r\n"
        "Last-Modified: Wed, 15 Nov 1995 04:58:08 GMT\r\n"
        "Content-Type: application/text\r\n"
        "Content-Length: 0\r\n",
        
        ""
        ) );

    messagesToParse.push_back( TestMessageData(
        200,
        "Date: Wed, 15 Nov 1995 06:25:24 GMT\r\n"
        "Last-Modified: Wed, 15 Nov 1995 04:58:08 GMT\r\n"
        "Content-Type: application/text\r\n",
        
        "5hhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhh"
        "hhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhh"
        "hhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhh6"
        ) );


    nx::Buffer sourceDataStream;
    int requestNumber = 0;
    for( const TestMessageData& msgData: messagesToParse )
    {
        sourceDataStream += "HTTP/1.1 "+nx::Buffer::number(msgData.statusCode)+" "+nx_http::StatusCode::toString(msgData.statusCode)+"\r\n";
        sourceDataStream += msgData.headers;
        sourceDataStream += "Test-Request-Number: "+nx::Buffer::number(requestNumber++)+"\r\n";
        if( !msgData.msgBody.isEmpty() )
            sourceDataStream += "Content-Length: "+nx::Buffer::number(msgData.msgBody.size())+"\r\n";
        sourceDataStream += "\r\n";
        sourceDataStream += msgData.msgBody;
    }

    static const int DATA_READ_STEP = 1;

    nx_http::HttpStreamReader streamReader;
    int messageNumber = 0;
    for( int pos = 0; pos < sourceDataStream.size(); pos += DATA_READ_STEP )
    {
        size_t bytesProcessed = 0;
        ASSERT_TRUE( streamReader.parseBytes( sourceDataStream.mid( pos ), DATA_READ_STEP, &bytesProcessed ) );
        ASSERT_EQ( streamReader.currentMessageNumber(), messageNumber );
        ASSERT_FALSE( streamReader.state() == nx_http::HttpStreamReader::parseError );
        if( streamReader.state() != nx_http::HttpStreamReader::messageDone )
            continue;
        ASSERT_EQ( streamReader.message().type, nx_http::MessageType::response );
        ASSERT_TRUE( streamReader.message().response != nullptr );
        ASSERT_EQ( streamReader.message().response->statusLine.statusCode, messagesToParse[messageNumber].statusCode );
        const QByteArray msgBody = streamReader.fetchMessageBody();
        ASSERT_EQ(
            messagesToParse[messageNumber].msgBody.size(),
            nx_http::getHeaderValue( streamReader.message().response->headers, "Content-Length" ).toInt() );
        ASSERT_TRUE( msgBody == messagesToParse[messageNumber].msgBody );
        ++messageNumber;
    }

    ASSERT_EQ( messageNumber, messagesToParse.size() );
}

//--gtest_filter=HttpStreamReader*
