#include <gtest/gtest.h>

#include <nx/network/http/http_types.h>

#include <nx/cloud/storage/client/aws_s3/aws_signature_v4.h>

namespace nx::cloud::storage::client::aws_s3::test {

class AwsSignatureV4:
    public ::testing::Test
{
public:
    AwsSignatureV4():
        m_credentials(
            "AKIAIOSFODNN7EXAMPLE",
            network::http::PasswordAuthToken("wJalrXUtnFEMI/K7MDENG/bPxRfiCYEXAMPLEKEY"))
    {
    }

protected:
    network::http::Credentials credentials() const
    {
        return m_credentials;
    }

    /**
     * Preparing request from example 1 at
     * https://docs.aws.amazon.com/AmazonS3/latest/API/sig-v4-header-based-auth.html
     */
    network::http::Request prepareGetRequest() const
    {
        nx::network::http::Request request;
        request.requestLine.method = nx::network::http::Method::get;
        request.requestLine.url = "/test.txt";
        request.requestLine.version = nx::network::http::http_1_1;

        request.headers.emplace("Host", "examplebucket.s3.amazonaws.com");
        request.headers.emplace("Range", "bytes=0-9");
        // Hex(SHA256Hash(""))
        request.headers.emplace(
            "x-amz-content-sha256",
            "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
        request.headers.emplace("x-amz-date", "20130524T000000Z");

        return request;
    }

    /**
     * Preparing request from example 2 at
     * https://docs.aws.amazon.com/AmazonS3/latest/API/sig-v4-header-based-auth.html
     */
    network::http::Request preparePutRequest() const
    {
        nx::network::http::Request request;
        request.requestLine.method = nx::network::http::Method::put;
        request.requestLine.url = "test$file.text";
        request.requestLine.version = nx::network::http::http_1_1;

        request.headers.emplace("Host", "examplebucket.s3.amazonaws.com");
        request.headers.emplace("Date", "Fri, 24 May 2013 00:00:00 GMT");
        request.headers.emplace("x-amz-date", "20130524T000000Z");
        request.headers.emplace("x-amz-storage-class", "REDUCED_REDUNDANCY");
        request.headers.emplace("x-amz-content-sha256", "44ce7dd67c959e0d3524ffac1771dfbba87d2b6b4b4e99e42034a8b803f8b072");

        request.messageBody = "Welcome to Amazon S3.";

        return request;
    }

    /**
     * Preparing request from example 3 at
     * https://docs.aws.amazon.com/AmazonS3/latest/API/sig-v4-header-based-auth.html
     */
    network::http::Request prepareGetRequestWithQuery1() const
    {
        nx::network::http::Request request;
        request.requestLine.method = nx::network::http::Method::get;
        request.requestLine.url = "?lifecycle";
        request.requestLine.version = nx::network::http::http_1_1;

        request.headers.emplace("Host", "examplebucket.s3.amazonaws.com");
        request.headers.emplace("x-amz-date", "20130524T000000Z");
        request.headers.emplace(
            "x-amz-content-sha256",
            "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");

        return request;
    }

    /**
     * Preparing request from example 4 at
     * https://docs.aws.amazon.com/AmazonS3/latest/API/sig-v4-header-based-auth.html
     */
    network::http::Request prepareGetRequestWithQuery2() const
    {
        nx::network::http::Request request;
        request.requestLine.method = nx::network::http::Method::get;
        request.requestLine.url = "?prefix=J&max-keys=2";
        request.requestLine.version = nx::network::http::http_1_1;

        request.headers.emplace("Host", "examplebucket.s3.amazonaws.com");
        request.headers.emplace("x-amz-date", "20130524T000000Z");
        request.headers.emplace(
            "x-amz-content-sha256",
            "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");

        return request;
    }

private:
    network::http::Credentials m_credentials;
};

TEST_F(AwsSignatureV4, GET_request_signature)
{
    const auto [signature, result] = SignatureCalculator::calculateSignature(
        prepareGetRequest(), credentials(), "us-east-1", "s3");

    ASSERT_TRUE(result);
    ASSERT_EQ(
        "f0e8bdb87c964420e857bd35b5d6ed310bd44f0170aba48dd91039c6036bdb41",
        signature);
}

TEST_F(AwsSignatureV4, GET_request_authorization_header)
{
    const auto [header, result] = SignatureCalculator::calculateAuthorizationHeader(
        prepareGetRequest(), credentials(), "us-east-1", "s3");

    ASSERT_TRUE(result);
    ASSERT_EQ(
        "AWS4-HMAC-SHA256 Credential=AKIAIOSFODNN7EXAMPLE/20130524/us-east-1/s3/aws4_request,"
        "SignedHeaders=host;range;x-amz-content-sha256;x-amz-date,"
        "Signature=f0e8bdb87c964420e857bd35b5d6ed310bd44f0170aba48dd91039c6036bdb41",
        header);
}

TEST_F(AwsSignatureV4, GET_request_with_query_1_authorization_header)
{
    const auto [header, result] = SignatureCalculator::calculateAuthorizationHeader(
        prepareGetRequestWithQuery1(), credentials(), "us-east-1", "s3");

    ASSERT_TRUE(result);
    ASSERT_EQ(
        "AWS4-HMAC-SHA256 Credential=AKIAIOSFODNN7EXAMPLE/20130524/us-east-1/s3/aws4_request,"
        "SignedHeaders=host;x-amz-content-sha256;x-amz-date,"
        "Signature=fea454ca298b7da1c68078a5d1bdbfbbe0d65c699e0f91ac7a200a0136783543",
        header);
}

TEST_F(AwsSignatureV4, GET_request_with_query_2_authorization_header)
{
    const auto [header, result] = SignatureCalculator::calculateAuthorizationHeader(
        prepareGetRequestWithQuery2(), credentials(), "us-east-1", "s3");

    ASSERT_TRUE(result);
    ASSERT_EQ(
        "AWS4-HMAC-SHA256 Credential=AKIAIOSFODNN7EXAMPLE/20130524/us-east-1/s3/aws4_request,"
        "SignedHeaders=host;x-amz-content-sha256;x-amz-date,"
        "Signature=34b48302e7b5fa45bde8084f4b7868a86f0a534bc59db6670ed5711ef69dc6f7",
        header);
}

TEST_F(AwsSignatureV4, PUT_request_authorization_header)
{
    const auto [header, result] = SignatureCalculator::calculateAuthorizationHeader(
        preparePutRequest(), credentials(), "us-east-1", "s3");

    ASSERT_TRUE(result);
    ASSERT_EQ(
        "AWS4-HMAC-SHA256 Credential=AKIAIOSFODNN7EXAMPLE/20130524/us-east-1/s3/aws4_request,"
        "SignedHeaders=date;host;x-amz-content-sha256;x-amz-date;x-amz-storage-class,"
        "Signature=98ad721746da40c64f1a55b78f14c238d841ea1380cd77a1b5971af0ece108bd",
        header);
}

} // namespace nx::cloud::storage::client::aws_s3::test
