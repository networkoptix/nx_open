/**
 * @file
 * Helper functions for calculating Http Digest (rfc2617).
 */

#pragma once

#include <QString>

#include "http_types.h"

namespace nx_http {

/**
 * Generates Authorization header and adds it to the request.
 */
bool NX_NETWORK_API addAuthorization(
    Request* const request,
    const StringType& userName,
    const boost::optional<StringType>& userPassword,
    const boost::optional<BufferType>& predefinedHA1,
    const header::WWWAuthenticate& wwwAuthenticateHeader);

BufferType NX_NETWORK_API calcHa1(
    const StringType& userName,
    const StringType& realm,
    const StringType& userPassword,
    const StringType& algorithm = {});
BufferType NX_NETWORK_API calcHa1(
    const QString& userName,
    const QString& realm,
    const QString& userPassword,
    const QString& algorithm = {});
BufferType NX_NETWORK_API calcHa1(
    const char* userName,
    const char* realm,
    const char* userPassword,
    const char* algorithm = "");

BufferType NX_NETWORK_API calcHa2(
    const StringType& method,
    const StringType& uri,
    const StringType& algorithm = {});

BufferType NX_NETWORK_API calcResponse(
    const BufferType& ha1,
    const BufferType& nonce,
    const BufferType& ha2,
    const StringType& algorithm = {});

BufferType NX_NETWORK_API calcResponseAuthInt(
    const BufferType& ha1,
    const BufferType& nonce,
    const StringType& nonceCount,
    const BufferType& clientNonce,
    const StringType& qop,
    const BufferType& ha2,
    const StringType& algorithm = {});

/**
 * NOTE: If predefinedHA1 is present then it is used. Otherwise, HA1 is calculated based on userPassword.
 */
bool NX_NETWORK_API calcDigestResponse(
    const StringType& method,
    const StringType& userName,
    const boost::optional<StringType>& userPassword,
    const boost::optional<BufferType>& predefinedHA1,
    const StringType& uri,
    const header::WWWAuthenticate& wwwAuthenticateHeader,
    header::DigestAuthorization* const digestAuthorizationHeader);

/**
 * To be used by server to validate recevied Authorization against known credentials.
 */
bool NX_NETWORK_API validateAuthorization(
    const StringType& method,
    const StringType& userName,
    const boost::optional<StringType>& userPassword,
    const boost::optional<BufferType>& predefinedHA1,
    const header::DigestAuthorization& digestAuthorizationHeader);

/**
 * @param ha1 That's what calcHa1 has returned.
 * WARNING: ha1.size() + 1 + nonce.size() MUST be divisible by 64! 
 *   This is requirement of MD5 algorithm.
 */
BufferType NX_NETWORK_API calcIntermediateResponse(
    const BufferType& ha1,
    const BufferType& nonce);

/**
 * Calculates MD5(ha1:nonce:ha2).
 * nonce is concatenation of nonceBase and nonceTrailer.
 * intermediateResponse is result of nx_http::calcIntermediateResponse.
 * @param intermediateResponse Calculated with calcIntermediateResponse.
 * @param intermediateResponseNonceLen Length of nonce (bytes) used to generate intermediateResponse.
 */
BufferType NX_NETWORK_API calcResponseFromIntermediate(
    const BufferType& intermediateResponse,
    size_t intermediateResponseNonceLen,
    const BufferType& nonceTrailer,
    const BufferType& ha2);

} // namespace nx_http
