/**********************************************************
* 25 may 2015
* a.kolesnikov
***********************************************************/

#ifndef NX_AUTH_TOOLS_H
#define NX_AUTH_TOOLS_H

#include <QString>

#include "httptypes.h"


namespace nx_http
{
    //Helper functions for calculating Http digest (rfc2617)

    //!Generates Authorization header and adds it to the \a request
    bool addAuthorization(
        Request* const request,
        const StringType& userName,
        const boost::optional<StringType>& userPassword,
        const boost::optional<BufferType>& predefinedHA1,
        const header::WWWAuthenticate& wwwAuthenticateHeader );

    BufferType calcHa1(
        const StringType& userName,
        const StringType& realm,
        const StringType& userPassword );
    /*!
        Covenience method
    */
    BufferType calcHa1(
        const QString& userName,
        const QString& realm,
        const QString& userPassword );
    BufferType calcHa1(
        const char* userName,
        const char* realm,
        const char* userPassword);

    BufferType calcHa2(
        const StringType& method,
        const StringType& uri );

    BufferType calcResponse(
        const BufferType& ha1,
        const BufferType& nonce,
        const BufferType& ha2 );

    BufferType calcResponseAuthInt(
        const BufferType& ha1,
        const BufferType& nonce,
        const StringType& nonceCount,
        const BufferType& clientNonce,
        const StringType& qop,
        const BufferType& ha2 );

    /*!
        If \a predefinedHA1 is present then it is used. Otherwise, HA1 is calculated based on \a userPassword
    */
    bool calcDigestResponse(
        const StringType& method,
        const StringType& userName,
        const boost::optional<StringType>& userPassword,
        const boost::optional<BufferType>& predefinedHA1,
        const StringType& uri,
        const header::WWWAuthenticate& wwwAuthenticateHeader,
        header::DigestAuthorization* const digestAuthorizationHeader );

    //!To be used by server to validate recevied Authorization against known credentials
    bool validateAuthorization(
        const StringType& method,
        const StringType& userName,
        const boost::optional<StringType>& userPassword,
        const boost::optional<BufferType>& predefinedHA1,
        const header::DigestAuthorization& digestAuthorizationHeader );

    /*!
        \param ha1 That's what \a calcHa1 has returned
        \warning ha1.size() + 1 + nonce.size() MUST be divisible by 64! This is requirement of MD5 algorithm
    */
    BufferType calcIntermediateResponse(
        const BufferType& ha1,
        const BufferType& nonce);
    /*!
        Calculates MD5(ha1:nonce:ha2).
        \a nonce is concatenation of \a nonceBase and \a nonceTrailer
        \a intermediateResponse is MD5_modified(ha1:nonceBase)
        \param intermediateResponse Calculated with \a calcIntermediateResponse
        \param intermediateResponseNonceLen Length of nonce (bytes) used to generate \a intermediateResponse
    */
    BufferType calcResponseFromIntermediate(
        const BufferType& intermediateResponse,
        size_t intermediateResponseNonceLen,
        const BufferType& nonceTrailer,
        const BufferType& ha2);
}

#endif  //NX_AUTH_TOOLS_H
