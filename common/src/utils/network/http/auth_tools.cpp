/**********************************************************
* 25 may 2015
* a.kolesnikov
***********************************************************/

#include "auth_tools.h"

#include <QtCore/QCryptographicHash>


namespace nx_http
{
    bool addAuthorization(
        Request* const request,
        const StringType& userName,
        const boost::optional<StringType>& userPassword,
        const boost::optional<BufferType>& predefinedHA1,
        const header::WWWAuthenticate& wwwAuthenticateHeader )
    {
        if( wwwAuthenticateHeader.authScheme == header::AuthScheme::basic )
        {
            header::BasicAuthorization basicAuthorization( userName, userPassword ? userPassword.get() : StringType() );
            nx_http::insertOrReplaceHeader(
                &request->headers,
                nx_http::HttpHeader(
                    header::Authorization::NAME,
                    basicAuthorization.serialized() ) );
            return true;
        }
        else if( wwwAuthenticateHeader.authScheme == header::AuthScheme::digest )
        {
            header::DigestAuthorization digestAuthorizationHeader;
            //calculating authorization header
            if( !calcDigestResponse(
                    request->requestLine.method,
                    userName,
                    userPassword,
                    predefinedHA1,
                    request->requestLine.url,
                    wwwAuthenticateHeader,
                    &digestAuthorizationHeader ) )
            {
                return false;
            }
            insertOrReplaceHeader(
                &request->headers,
                nx_http::HttpHeader( header::Authorization::NAME, digestAuthorizationHeader.serialized() ) );
            return true;
        }

        return false;
    }

    BufferType calcHa1(
        const StringType& userName,
        const StringType& realm,
        const StringType& userPassword )
    {
        QCryptographicHash md5HashCalc( QCryptographicHash::Md5 );
        md5HashCalc.addData( userName );
        md5HashCalc.addData( ":" );
        md5HashCalc.addData( realm );
        md5HashCalc.addData( ":" );
        md5HashCalc.addData( userPassword );
        return md5HashCalc.result().toHex();
    }

    BufferType calcHa1(
        const QString& userName,
        const QString& realm,
        const QString& userPassword )
    {
        return calcHa1( userName.toUtf8(), realm.toUtf8(), userPassword.toUtf8() );
    }

    /*!
        \note HA2 in case of qop=auth-int is not supported
    */
    BufferType calcHa2(
        const StringType& method,
        const StringType& uri )
    {
        QCryptographicHash md5HashCalc( QCryptographicHash::Md5 );
        md5HashCalc.addData( method );
        md5HashCalc.addData( ":" );
        md5HashCalc.addData( uri );
        return md5HashCalc.result().toHex();
    }

    /*!
        \return response in hex representation
    */
    BufferType calcResponse(
        const BufferType& ha1,
        const BufferType& nonce,
        const BufferType& ha2 )
    {
        QCryptographicHash md5HashCalc( QCryptographicHash::Md5 );
        md5HashCalc.addData( ha1 );
        md5HashCalc.addData( ":" );
        md5HashCalc.addData( nonce );
        md5HashCalc.addData( ":" );
        md5HashCalc.addData( ha2 );
        return md5HashCalc.result().toHex();
    }

    //!Calculate Digest response with message-body validation (auth-int)
    BufferType calcResponseAuthInt(
        const BufferType& ha1,
        const BufferType& nonce,
        const StringType& nonceCount,
        const BufferType& clientNonce,
        const StringType& qop,
        const BufferType& ha2 )
    {
        QCryptographicHash md5HashCalc( QCryptographicHash::Md5 );
        md5HashCalc.addData( ha1 );
        md5HashCalc.addData( ":" );
        md5HashCalc.addData( nonce );
        md5HashCalc.addData( ":" );
        md5HashCalc.addData( nonceCount );
        md5HashCalc.addData( ":" );
        md5HashCalc.addData( clientNonce );
        md5HashCalc.addData( ":" );
        md5HashCalc.addData( qop );
        md5HashCalc.addData( ":" );
        md5HashCalc.addData( ha2 );
        return md5HashCalc.result().toHex();
    }


    namespace
    {
        bool calcDigestResponse(
            const QByteArray& method,
            const StringType& userName,
            const boost::optional<StringType>& userPassword,
            const boost::optional<BufferType>& predefinedHA1,
            const QUrl& url,
            const QMap<BufferType, BufferType>& inputParams,
            QMap<BufferType, BufferType>* const outputParams )
        {
            //reading params
            QMap<BufferType, BufferType>::const_iterator nonceIter = inputParams.find( "nonce" );
            const BufferType nonce = nonceIter != inputParams.end() ? nonceIter.value() : BufferType();
            QMap<BufferType, BufferType>::const_iterator realmIter = inputParams.find( "realm" );
            const BufferType realm = realmIter != inputParams.end() ? realmIter.value() : BufferType();
            QMap<BufferType, BufferType>::const_iterator qopIter = inputParams.find( "qop" );
            const BufferType qop = qopIter != inputParams.end() ? qopIter.value() : BufferType();

            if( qop.indexOf( "auth-int" ) != -1 ) //TODO #ak qop can have value "auth,auth-int". That should be supported
                return false;   //qop=auth-int is not supported

            const BufferType& ha1 = predefinedHA1
                ? predefinedHA1.get()
                : calcHa1(
                    userName,
                    realm,
                    userPassword ? userPassword.get() : QByteArray() );
            //HA2, qop=auth-int is not supported
            const BufferType& ha2 = calcHa2(
                method,
                url.path().toLatin1() );
            //response
            outputParams->insert( "username", userName );
            outputParams->insert( "realm", realm );
            outputParams->insert( "nonce", nonce );
            outputParams->insert( "uri", url.path().toLatin1() );

            const BufferType nonceCount = "00000001";     //TODO #ak generate it
            const BufferType clientNonce = "0a4f113b";    //TODO #ak generate it

            QByteArray digestResponse;
            if( qop.isEmpty() )
            {
                digestResponse = calcResponse( ha1, nonce, ha2 );
            }
            else
            {
                digestResponse = calcResponseAuthInt( ha1, nonce, nonceCount, clientNonce, qop, ha2 );
                outputParams->insert( "qop", qop );
                outputParams->insert( "nc", nonceCount );
                outputParams->insert( "cnonce", clientNonce );
            }
            outputParams->insert( "response", digestResponse );
            return true;
        }
    }

    bool calcDigestResponse(
        const QByteArray& method,
        const StringType& userName,
        const boost::optional<StringType>& userPassword,
        const boost::optional<BufferType>& predefinedHA1,
        const QUrl& url,
        const header::WWWAuthenticate& wwwAuthenticateHeader,
        header::DigestAuthorization* const digestAuthorizationHeader )
    {
        if( wwwAuthenticateHeader.authScheme != header::AuthScheme::digest )
            return false;

        //TODO #ak have to set digestAuthorizationHeader->digest->userid somewhere

        return calcDigestResponse(
            method,
            userName,
            userPassword,
            predefinedHA1,
            url,
            wwwAuthenticateHeader.params,
            &digestAuthorizationHeader->digest->params );
    }

    bool validateAuthorization(
        const StringType& method,
        const StringType& userName,
        const boost::optional<StringType>& userPassword,
        const boost::optional<BufferType>& predefinedHA1,
        const QUrl& url,
        const header::DigestAuthorization& digestAuthorizationHeader )
    {
        QMap<BufferType, BufferType> outputParams;
        if( !calcDigestResponse(
                method,
                userName,
                userPassword,
                predefinedHA1,
                url,
                digestAuthorizationHeader.digest->params,
                &outputParams ) )
            return false;
        auto calculatedResponseIter = outputParams.find( "response" );
        if( calculatedResponseIter == outputParams.end() )
            return false;
        auto responseIter = digestAuthorizationHeader.digest->params.find( "response" );
        if( responseIter == digestAuthorizationHeader.digest->params.end() )
            return false;
        return calculatedResponseIter.value() == responseIter.value();
    }
}
