// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <nx/utils/random.h>
#include <nx/string.h>

#include <nx/vms/api/data/system_merge_history_record.h>

namespace ec2 {
namespace test {

class ApiSystemMergeHistoryRecordSignature:
    public ::testing::Test
{
protected:
    void givenRandomRecord()
    {
        m_authKey = nx::utils::random::generateName(7);

        m_record.timestamp = nx::utils::random::number<int>();
        m_record.mergedSystemLocalId = nx::utils::random::generateName<QByteArray>(7);
        m_record.mergedSystemCloudId = nx::utils::random::generateName<QByteArray>(7);
        m_record.username = nx::utils::random::generateName<QByteArray>(7);
    }

    void givenRandomSignedRecord()
    {
        givenRandomRecord();
        signRecord();
    }

    void whenCorruptSignature()
    {
        m_record.signature = nx::utils::random::generateName<QByteArray>(7);
    }

    void thenRecordCanNoLongerBeVerified()
    {
        ASSERT_FALSE(m_record.verify(m_authKey));
    }

    void signRecord()
    {
        m_record.sign(m_authKey);
    }

    void assertRecordSignatureCorrect()
    {
        ASSERT_TRUE(m_record.verify(m_authKey));
    }

private:
    nx::vms::api::SystemMergeHistoryRecord m_record;
    nx::String m_authKey;
};

TEST_F(ApiSystemMergeHistoryRecordSignature, valid_signature_is_verified)
{
    givenRandomSignedRecord();
    assertRecordSignatureCorrect();
}

TEST_F(ApiSystemMergeHistoryRecordSignature, invalid_signature_is_not_verified)
{
    givenRandomSignedRecord();
    whenCorruptSignature();
    thenRecordCanNoLongerBeVerified();
}

} // namespace test
} // namespace ec2
