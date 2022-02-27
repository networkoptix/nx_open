// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <licensing/license.h>
#include <nx_ec/data/api_conversion_functions.h>

#include <nx/vms/api/data/license_data.h>

#include <nx/fusion/model_functions.h>

namespace {

// License block with minimum viable set of fields.
static const QByteArray kMinimalLicenseBlock =
    "NAME=hdwitness\n"
    "SERIAL=NA7R-3B29-QZMR-9WUG\n"
    "HWID=050affb9b8dbf953733b60493a284840fa\n"
    "COUNT=1\n"
    "CLASS=digital\n"
    "VERSION=4.1.0.0\n"
    "BRAND=hdwitness\n"
    "EXPIRATION=\n"
    "SIGNATURE2=OcMzz57qWCUxe6eyvH2bFdDYoLW63gbhTVe73bmBBM9jKjgzRAHa3U7dwZAka8NJwRJR2jyDFZPttvFluX1WEg==\n";

// License block with all available custom fields.
static const QByteArray kFullLicenseBlock =
    "NAME=hdwitness\n"
    "SERIAL=NA7R-3B29-QZMR-9WUG\n"
    "HWID=050affb9b8dbf953733b60493a284840fa\n"
    "COUNT=1\n"
    "CLASS=digital\n"
    "VERSION=4.1.0.0\n"
    "BRAND=hdwitness\n"
    "EXPIRATION=\n"
    "SIGNATURE2=OcMzz57qWCUxe6eyvH2bFdDYoLW63gbhTVe73bmBBM9jKjgzRAHa3U7dwZAka8NJwRJR2jyDFZPttvFluX1WEg==\n"
    "COMPANY=Company 1\n"
    "SUPPORT=user1@companyone.com\n"
    "DEACTIVATIONS=1\n";

static const QByteArray kLicenseServerResponseWithRegionalSupport = R"json(
    {
        "name": "hdwitness",
        "key": "NA7R-3B29-QZMR-9WUG",
        "hardwareId": "050affb9b8dbf953733b60493a284840fa",
        "cameraCount": 1,
        "licenseType": "digital",
        "version": "4.1.0.0",
        "brand": "hdwitness",
        "expiration": "",
        "signature": "OcMzz57qWCUxe6eyvH2bFdDYoLW63gbhTVe73bmBBM9jKjgzRAHa3U7dwZAka8NJwRJR2jyDFZPttvFluX1WEg==",
        "company": "Company 1",
        "support": "user1@companyone.com"
    }
)json";

static const QByteArray kLicenseServerResponseWithDeactivations = R"json(
    {
        "name": "hdwitness",
        "key": "NA7R-3B29-QZMR-9WUG",
        "hardwareId": "050affb9b8dbf953733b60493a284840fa",
        "cameraCount": 1,
        "licenseType": "digital",
        "version": "4.1.0.0",
        "brand": "hdwitness",
        "expiration": "",
        "signature": "OcMzz57qWCUxe6eyvH2bFdDYoLW63gbhTVe73bmBBM9jKjgzRAHa3U7dwZAka8NJwRJR2jyDFZPttvFluX1WEg==",
        "deactivations": 1
    }
)json";

} // namespace

namespace nx::vms::license {

/**
 * Make sure license structure can be safely converted to the api data class and back.
 */
TEST(License, conversionAndEquality)
{
    QnLicensePtr licenseFromBlock(new QnLicense(kFullLicenseBlock));
    nx::vms::api::DetailedLicenseData convertedLicenseData;
    ec2::fromResourceToApi(licenseFromBlock, convertedLicenseData);
    QnLicense licenseFromData(convertedLicenseData);

    EXPECT_EQ(licenseFromBlock->toString(), licenseFromData.toString());
    ASSERT_EQ(licenseFromBlock->name(), licenseFromData.name());
    ASSERT_EQ(licenseFromBlock->key(), licenseFromData.key());
    ASSERT_EQ(licenseFromBlock->cameraCount(), licenseFromData.cameraCount());
    ASSERT_EQ(licenseFromBlock->hardwareId(), licenseFromData.hardwareId());
    ASSERT_EQ(licenseFromBlock->signature(), licenseFromData.signature());
    ASSERT_EQ(licenseFromBlock->xclass(), licenseFromData.xclass());
    ASSERT_EQ(licenseFromBlock->version(), licenseFromData.version());
    ASSERT_EQ(licenseFromBlock->brand(), licenseFromData.brand());
    ASSERT_EQ(licenseFromBlock->expiration(), licenseFromData.expiration());
    ASSERT_EQ(licenseFromBlock->orderType(), licenseFromData.orderType());
    ASSERT_EQ(licenseFromBlock->regionalSupport().company, licenseFromData.regionalSupport().company);
    ASSERT_EQ(licenseFromBlock->regionalSupport().address, licenseFromData.regionalSupport().address);
    ASSERT_EQ(licenseFromBlock->deactivationsCount(), licenseFromData.deactivationsCount());
}

/**
 * Server should update old licenses with new ones on start.
 */
TEST(License, updateExistingLicenseWithRegionalSupport)
{
    QnLicensePtr existingLicense(new QnLicense(kMinimalLicenseBlock));
    nx::vms::api::DetailedLicenseData existingLicenseData;
    ec2::fromResourceToApi(existingLicense, existingLicenseData);

    nx::vms::api::DetailedLicenseData updatedLicenseData;
    EXPECT_TRUE(QJson::deserialize(kLicenseServerResponseWithRegionalSupport, &updatedLicenseData));

    // If licenses are not equal, old license will be overwritten with a new one.
    ASSERT_NE(existingLicenseData, updatedLicenseData);
    ASSERT_EQ(updatedLicenseData.company, "Company 1");
    ASSERT_EQ(updatedLicenseData.support, "user1@companyone.com");
}

/**
 * Server should update old licenses with new ones on start.
 */
TEST(License, updateExistingLicenseWithDeactivationsCount)
{
    QnLicensePtr existingLicense(new QnLicense(kMinimalLicenseBlock));
    nx::vms::api::DetailedLicenseData existingLicenseData;
    ec2::fromResourceToApi(existingLicense, existingLicenseData);

    nx::vms::api::DetailedLicenseData updatedLicenseData;
    EXPECT_TRUE(QJson::deserialize(kLicenseServerResponseWithDeactivations, &updatedLicenseData));

    // If licenses are not equal, old license will be overwritten with a new one.
    ASSERT_NE(existingLicenseData, updatedLicenseData);
    ASSERT_EQ(updatedLicenseData.deactivations, 1);
}

} // namespace nx::vms::license::test
