#pragma once

#include <nx/cloud/aws/test_support/aws_s3_emulator.h>

namespace nx::cloud::storage::service::test {

class S3Bucket: public aws::s3::test::AwsS3Emulator
{
    using base_type = aws::s3::test::AwsS3Emulator;

public:
    S3Bucket(
        std::string location = aws::s3::test::kDefaultS3Location,
        std::string name = std::string(),
        bool local = true);
    ~S3Bucket();

    /**
     * @return the name of the bucket
     */
    const std::string& name() const;

    /**
     * @return the aws url for this bucket, e.g. http://bucketName.s3.amazonaws.com
     */
    nx::utils::Url url() const;

private:
    QString hostName() const;

private:
    const std::string m_name;
};

} // namespace nx::cloud::storage::service::test