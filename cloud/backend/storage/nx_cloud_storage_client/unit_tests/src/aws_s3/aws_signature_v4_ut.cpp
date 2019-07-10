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

    network::http::Request prepareGetRequest() const
    {
        nx::network::http::Request request;
        request.requestLine.method = nx::network::http::Method::get;
        request.requestLine.url = "/test.txt";
        request.requestLine.version = nx::network::http::http_1_1;

        request.headers.emplace("Host", "examplebucket.s3.amazonaws.com");
        // request.headers.emplace("Authorization": "SignatureToBeCalculated");
        request.headers.emplace("Range", "bytes=0-9");
        // Hex(SHA256Hash(""))
        request.headers.emplace("x-amz-content-sha256", "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
        request.headers.emplace("x-amz-date", "20130524T000000Z");

        return request;
    }

private:
    network::http::Credentials m_credentials;
};

TEST_F(AwsSignatureV4, GET_request_signature)
{
    const auto signature = SignatureCalculator::calculateSignature(
        prepareGetRequest(), credentials(), "us-east-1", "s3");

    ASSERT_EQ(
        "f0e8bdb87c964420e857bd35b5d6ed310bd44f0170aba48dd91039c6036bdb41",
        signature);
}

TEST_F(AwsSignatureV4, GET_request_authorization_header)
{
    const auto header = SignatureCalculator::calculateAuthorizationHeader(
        prepareGetRequest(), credentials(), "us-east-1", "s3");

    ASSERT_EQ(
        "AWS4-HMAC-SHA256 Credential=AKIAIOSFODNN7EXAMPLE/20130524/us-east-1/s3/aws4_request,"
        "SignedHeaders=host;range;x-amz-content-sha256;x-amz-date,"
        "Signature=f0e8bdb87c964420e857bd35b5d6ed310bd44f0170aba48dd91039c6036bdb41",
        header);
}

} // namespace nx::cloud::storage::client::aws_s3::test
