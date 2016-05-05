/**********************************************************
* 25 may 2015
* a.kolesnikov
***********************************************************/

#include "auth_tools.h"

#include <openssl/md5.h>

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
                    request->requestLine.url.path().toUtf8(),
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

    BufferType calcHa1(
        const char* userName,
        const char* realm,
        const char* userPassword)
    {
        return calcHa1(StringType(userName), StringType(realm), StringType(userPassword));
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
            const StringType& uri,
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
                uri );
            //response
            outputParams->insert( "username", userName );
            outputParams->insert( "realm", realm );
            outputParams->insert( "nonce", nonce );
            outputParams->insert( "uri", uri );

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
        const StringType& uri,
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
            uri,
            wwwAuthenticateHeader.params,
            &digestAuthorizationHeader->digest->params );
    }

    bool validateAuthorization(
        const StringType& method,
        const StringType& userName,
        const boost::optional<StringType>& userPassword,
        const boost::optional<BufferType>& predefinedHA1,
        const header::DigestAuthorization& digestAuthorizationHeader )
    {
        auto uriIter = digestAuthorizationHeader.digest->params.find( "uri" );
        if( uriIter == digestAuthorizationHeader.digest->params.end() )
            return false;

        QMap<BufferType, BufferType> outputParams;
        if( !calcDigestResponse(
                method,
                userName,
                userPassword,
                predefinedHA1,
                uriIter.value(),
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

    static const size_t MD5_CHUNK_LEN = 64;

    BufferType calcIntermediateResponse(
        const BufferType& ha1,
        const BufferType& nonce)
    {
        NX_ASSERT((ha1.size() + 1 + nonce.size()) % MD5_CHUNK_LEN == 0);

        MD5_CTX md5Ctx;
        MD5_Init(&md5Ctx);
        MD5_Update(&md5Ctx, ha1.constData(), ha1.size());
        MD5_Update(&md5Ctx, ":", 1);
        MD5_Update(&md5Ctx, nonce.constData(), nonce.size());
        BufferType intermediateResponse;
        intermediateResponse.resize(MD5_DIGEST_LENGTH);
        memcpy(intermediateResponse.data(), &md5Ctx, MD5_DIGEST_LENGTH);
        return intermediateResponse.toHex();
    }

    BufferType calcResponseFromIntermediate(
        const BufferType& intermediateResponse,
        size_t intermediateResponseNonceLen,
        const BufferType& nonceTrailer,
        const BufferType& ha2)
    {
        NX_ASSERT((MD5_DIGEST_LENGTH*2 + 1 + intermediateResponseNonceLen) % MD5_CHUNK_LEN == 0);

        const auto intermediateResponseBin = QByteArray::fromHex(intermediateResponse);

        MD5_CTX md5Ctx;
        memset(&md5Ctx, 0, sizeof(md5Ctx));
        memcpy(&md5Ctx, intermediateResponseBin.constData(), MD5_DIGEST_LENGTH);
        md5Ctx.Nl = (MD5_DIGEST_LENGTH * 2 + 1 + intermediateResponseNonceLen) << 3;
        //as if we have just passed ha1:nonceBase to MD5_Update
        MD5_Update(&md5Ctx, nonceTrailer.constData(), nonceTrailer.size());
        MD5_Update(&md5Ctx, ":", 1);
        MD5_Update(&md5Ctx, ha2.constData(), ha2.size());
        BufferType response;
        response.resize(MD5_DIGEST_LENGTH);
        MD5_Final(reinterpret_cast<unsigned char*>(response.data()), &md5Ctx);
        return response.toHex();
    }
}
