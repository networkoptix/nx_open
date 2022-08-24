// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <licensing/license.h>
#include <nx_ec/data/api_conversion_functions.h>
#include <nx/fusion/model_functions.h>
#include <nx/vms/api/data/cloud_license_data.h>

namespace nx::vms::license::test {

// Make sure license structure can be safely converted to the api data class and back.
TEST(CloudLicense, serialize)
{
    using namespace nx::vms::api;

    static const QByteArray serializedData = R"json(
	{
		"version" : "2.0",

		"params" : {

			"services" : {
				"cloudStorage" : {
					"cloudStorageSize" : "100GB"
				},
				"localRecording" : {
					"totalChannelNumber" : "10"
				}
			},

			"orderParams" : {
				"licenseKey" : "2W36-ELIH-42VB-UO9G",
				"brand" : "nx",
				"licensePeriod" : "2019-04-25 23:59:59",
				"totalDeactivationNumber" : 10
			},

			"masterSignature" : "ABCDEFGH"
		},

		"state" : {
			"licenseState" : "active",
			"cloudSystemId" : "{F5504DBB-92FA-4BDC-931C-C385098F9515}",
			"firstActivationDate" : "2010-01-25 12:04:05",
			"lastActivationDate" : "2020-01-25 12:04:05",
			"expirationDate" : "2025-11-30 12:04:05",
			"deactivationsRemaining" : 3
		},

		"security" : {
			"checkPeriod" : 86400,
			"lastCheck" : "2010-01-25 12:05:05",
			"tmpExpirationDate" : "2010-01-24 12:04:05",
			"signature" : "ABSCDEF",
			"issue" : "ok"
		}
	}
	)json";

    CloudLicenseData license;
    std::map<QString, QString> data;
    data.emplace("space1", "10");
    data.emplace("space2", "20");
    license.params.services.emplace("localRecording", data);
    license.params.services.emplace("cloudStorage", data);
    QByteArray json = QByteArray::fromStdString(nx::reflect::json::serialize(license));
	ASSERT_TRUE(json.contains("space1"));
	ASSERT_TRUE(json.contains("localRecording"));
	ASSERT_TRUE(json.contains("cloudStorage"));

	license = CloudLicenseData();
	ASSERT_TRUE(nx::reflect::json::deserialize(
		std::string_view(serializedData.data(), serializedData.size()), &license));

	ASSERT_TRUE(license.params.services.contains("localRecording"));
	ASSERT_TRUE(license.params.services.contains("cloudStorage"));
	ASSERT_EQ("10", license.params.services["localRecording"]["totalChannelNumber"]);
	ASSERT_EQ(2, license.version.major());
	ASSERT_EQ(0, license.version.minor());

	ASSERT_EQ("2W36-ELIH-42VB-UO9G", license.params.orderParams.licenseKey);
	ASSERT_EQ("nx", license.params.orderParams.brand);
	ASSERT_EQ("2019-04-25 23:59:59", license.params.orderParams.licensePeriod);
	ASSERT_EQ(10, license.params.orderParams.totalDeactivationNumber);
	ASSERT_EQ("ABCDEFGH", license.params.masterSignature);

#if 0
"licenseState" : {
			"state" : "active",
				"cloudSystemId" : "{F5504DBB-92FA-4BDC-931C-C385098F9515}",
				"firstActivationDate" : "2010-01-25 12:04:05",
				"lastActivationDate" : "2020-01-25 12:04:05",
				"expiration_date" : "2025-11-30 12:04:05",
				"deactivationRemaining" : "3"
		},

			"security" : {
				"checkPeriod" : "168H",
					"lastCheck" : "2010-01-25 12:05:05",
					"tmpExpirationDate" : "2010-01-24 12:04:05",
					"signature" : "ABSCDEF",
					"issue" : "ok"
#endif
	
}

} // namespace nx::vms::license::test
