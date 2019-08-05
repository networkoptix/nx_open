#pragma once

#include <nx_ec/data/api_fwd.h>
#include <nx/vms/api/data/timestamp.h>
#include <nx/clusterdb/engine/command_data.h>

namespace ec2 { class QnAbstractTransaction; }

namespace nx::test {

bool compare(
    const nx::vms::api::Timestamp& vmsTimestamp,
    const nx::clusterdb::engine::Timestamp& cloudTimestamp);

bool compare(
    const ::ec2::QnAbstractTransaction& vmsTran,
    const nx::clusterdb::engine::CommandHeader& cloudCommandHeader);

bool compare(
    const ::ec2::ApiTransactionData& vmsTran,
    const nx::clusterdb::engine::CommandData& cloudCommand); 

bool compare(
    ::ec2::ApiTransactionDataList vmsTransactionLog,
    nx::clusterdb::engine::CommandDataList cloudCommandLog);

} // namespace nx::test
