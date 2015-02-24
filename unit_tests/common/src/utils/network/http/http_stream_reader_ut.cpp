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
    bool chunked;

    TestMessageData(
        int _statusCode,
        nx::Buffer _headers,
        nx::Buffer _msgBody )
    :
        statusCode( _statusCode ),
        headers( _headers ),
        msgBody( _msgBody ),
        chunked( false )
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


    messagesToParse.push_back( TestMessageData(
        200,
        "guid: {47bf37a0-72a6-2890-b967-5da9c390d28a}\r\n"
        "Content-Type: application/octet-stream\r\n"
        "runtime-guid: {cff70ae1-01af-4e84-ab4d-ed8a28fab3bb}\r\n"
        "Content-Length: 0\r\n"
        "NX-EC-SYSTEM-NAME: ak_ec_2.3\r\n"
        "NX-EC-PROTO-VERSION: 1008\r\n"
        "system-identity-time: 0\r\n"
        "Access-Control-Allow-Origin: *\r\n",

        ""
        ) );

    messagesToParse.push_back( TestMessageData(
        200,
        "guid: {47bf37a0-72a6-2890-b967-5da9c390d28a}\r\n"
        "Content-Type: application/octet-stream\r\n"
        "runtime-guid: {cff70ae1-01af-4e84-ab4d-ed8a28fab3bb}\r\n"
        "NX-EC-SYSTEM-NAME: ak_ec_2.3\r\n"
        "Transfer-Encoding: chunked\r\n"
        "NX-EC-PROTO-VERSION: 1008\r\n"
        "system-identity-time: 0\r\n"
        "Access-Control-Allow-Origin: *\r\n",

        "0000020\r\n"
        "1xxxxxxxxxxxxxxxxxxxxxxxxxxxxxx2"
        "\r\n"
        "0000040\r\n"
        "3xxxxxxxxxxxxxxxxxxxxxxxxxxxxxx4"
        "5xxxxxxxxxxxxxxxxxxxxxxxxxxxxxx6"
        "\r\n"
        "0000060\r\n"
        "7xxxxxxxxxxxxxxxxxxxxxxxxxxxxxx8"
        "9xxxxxxxxxxxxxxxxxxxxxxxxxxxxx10"
        "11xxxxxxxxxxxxxxxxxxxxxxxxxxxx12"
        "\r\n"
        "0\r\n"
        "\r\n"
        ) );



    messagesToParse.push_back( TestMessageData(
        401,
        "Content-Type: text/html\r\n"
        "x-server-guid: {47bf37a0-72a6-2890-b967-5da9c390d28a}\r\n"
        "WWW-Authenticate: Digest realm=\"NetworkOptix\",nonce=\"50df1b6e2a378\"\r\n"
        "Access-Control-Allow-Origin: *\r\n",

        "1xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
        "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
        "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
        "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
        "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
        "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
        "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
        "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
        "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
        "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
        "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
        "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
        "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
        "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
        "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
        "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
        "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
        "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
        "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
        "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
        "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
        "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
        "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
        "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
        "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx2"
        ) );



    messagesToParse.push_back( TestMessageData(
        401,
        "Content-Type: text/html\r\n"
        "x-server-guid: {47bf37a0-72a6-2890-b967-5da9c390d28a}\r\n"
        "WWW-Authenticate: Digest realm=\"NetworkOptix\",nonce=\"50df1b6e2b700\"\r\n"
        "Access-Control-Allow-Origin: *\r\n",

        "1xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
        "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
        "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
        "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
        "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
        "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
        "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
        "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
        "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
        "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
        "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
        "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
        "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
        "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
        "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
        "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
        "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
        "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
        "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
        "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
        "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
        "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
        "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
        "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
        "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx2"
        ) );

    messagesToParse.push_back( TestMessageData(
        401,
        "Content-Type: text/html\r\n"
        "x-server-guid: {47bf37a0-72a6-2890-b967-5da9c390d28a}\r\n"
        "WWW-Authenticate: Digest realm=\"NetworkOptix\",nonce=\"50df1b6e2ca88\"\r\n"
        "Access-Control-Allow-Origin: *\r\n",

        "1xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
        "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
        "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
        "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
        "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
        "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
        "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
        "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
        "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
        "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
        "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
        "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
        "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
        "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
        "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
        "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
        "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
        "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
        "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
        "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
        "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
        "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
        "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
        "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
        "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx2"
        ) );

    messagesToParse.push_back( TestMessageData(
        200,
        "guid: {47bf37a0-72a6-2890-b967-5da9c390d28a}\r\n"
        "Content-Type: application/octet-stream\r\n"
        "runtime-guid: {cff70ae1-01af-4e84-ab4d-ed8a28fab3bb}\r\n"
        "Content-Length: 0\r\n"
        "NX-EC-SYSTEM-NAME: ak_ec_2.3\r\n"
        "NX-EC-PROTO-VERSION: 1008\r\n"
        "system-identity-time: 0\r\n"
        "Access-Control-Allow-Origin: *\r\n",

        "" ) );


    messagesToParse.push_back( TestMessageData(
        200,
        "guid: {47bf37a0-72a6-2890-b967-5da9c390d28a}\r\n"
        "Content-Type: application/octet-stream\r\n"
        "runtime-guid: {cff70ae1-01af-4e84-ab4d-ed8a28fab3bb}\r\n"
        "NX-EC-SYSTEM-NAME: ak_ec_2.3\r\n"
        "Transfer-Encoding: chunked\r\n"
        "NX-EC-PROTO-VERSION: 1008\r\n"
        "system-identity-time: 0\r\n"
        "Access-Control-Allow-Origin: *\r\n",


        "0000020;time_sync_6248_1422706690167_17640391744334\r\n"
        "1xxxxxxxxxxxxxxxxxxxxxxxxxxxxxx2"
        "\r\n"
        "0000040;time_sync_6248_1422706690167_17640391744334\r\n"
        "3xxxxxxxxxxxxxxxxxxxxxxxxxxxxxx4"
        "5xxxxxxxxxxxxxxxxxxxxxxxxxxxxxx6"
        "\r\n"
        "0000060;time_sync_6248_1422706690167_17640391744334\r\n"
        "7xxxxxxxxxxxxxxxxxxxxxxxxxxxxxx8"
        "9xxxxxxxxxxxxxxxxxxxxxxxxxxxxx10"
        "11xxxxxxxxxxxxxxxxxxxxxxxxxxxx12"
        "\r\n"
        "0\r\n"
        "\r\n"
        ) );

    nx::Buffer sourceDataStream;
    int requestNumber = 0;
    for( TestMessageData& msgData: messagesToParse )
    {
        sourceDataStream += "HTTP/1.1 "+nx::Buffer::number(msgData.statusCode)+" "+nx_http::StatusCode::toString(msgData.statusCode)+"\r\n";
        sourceDataStream += msgData.headers;
        sourceDataStream += "Test-Request-Number: "+nx::Buffer::number(requestNumber++)+"\r\n";
        msgData.chunked = msgData.headers.indexOf("Transfer-Encoding: chunked") != -1;
        if( !msgData.msgBody.isEmpty() && !msgData.chunked )
            sourceDataStream += "Content-Length: "+nx::Buffer::number(msgData.msgBody.size())+"\r\n";
        sourceDataStream += "\r\n";
        sourceDataStream += msgData.msgBody;
    }

    for( int dataStep = 1; dataStep < 16*1024; dataStep <<= 1 )
    {
        nx_http::HttpStreamReader streamReader;
        int messageNumber = 0;
        for( int pos = 0; pos < sourceDataStream.size(); )
        {
            size_t bytesProcessed = 0;
            ASSERT_TRUE( streamReader.parseBytes( 
                            sourceDataStream.mid( pos ),
                            std::min<int>( dataStep, sourceDataStream.size() - pos ),
                            &bytesProcessed ) );
            //ASSERT_TRUE( bytesProcessed > 0 );
            pos += bytesProcessed;
            ASSERT_EQ( streamReader.currentMessageNumber(), messageNumber );
            ASSERT_FALSE( streamReader.state() == nx_http::HttpStreamReader::parseError );
            if( streamReader.state() == nx_http::HttpStreamReader::readingMessageBody )
            {
                ASSERT_EQ( streamReader.message().type, nx_http::MessageType::response );
            }
            if( streamReader.state() != nx_http::HttpStreamReader::messageDone )
                continue;
            ASSERT_EQ( streamReader.message().type, nx_http::MessageType::response );
            ASSERT_TRUE( streamReader.message().response != nullptr );
            ASSERT_EQ( streamReader.message().response->statusLine.statusCode, messagesToParse[messageNumber].statusCode );
            const QByteArray msgBody = streamReader.fetchMessageBody();
            if( !messagesToParse[messageNumber].chunked )
            {
                ASSERT_EQ(
                    messagesToParse[messageNumber].msgBody.size(),
                    nx_http::getHeaderValue( streamReader.message().response->headers, "Content-Length" ).toInt() );
                ASSERT_TRUE( msgBody == messagesToParse[messageNumber].msgBody );
            }
            else
            {
                //TODO #ak check chunked message body
            }
            ++messageNumber;
        }

        ASSERT_EQ( messageNumber, messagesToParse.size() );
    }
}
