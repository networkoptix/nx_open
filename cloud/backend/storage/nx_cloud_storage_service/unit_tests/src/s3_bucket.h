#pragma once

#include <nx/cloud/aws/test_support/aws_s3_emulator.h>

namespace nx::cloud::storage::service::test {

class S3Bucket: public aws::test::AwsS3Emulator
{
    using base_type = aws::test::AwsS3Emulator;

public:
    S3Bucket(const std::string& name, std::string location = aws::test::kDefaultS3Location);
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