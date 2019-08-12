#pragma once

#include "../aws_signature_v4.h"

namespace nx::cloud::aws::s3 {

class NX_AWS_CLIENT_API SignatureCalculator:
    public aws::SignatureCalculator
{
protected:
    virtual void hashPath(nx::utils::QnCryptographicHash* hash, const QString& path) override;
};

} // namespace nx::cloud::aws::s3
