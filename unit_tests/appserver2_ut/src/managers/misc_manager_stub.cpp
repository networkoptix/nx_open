#include "misc_manager_stub.h"

namespace ec2 {
namespace test {

int MiscManagerStub::changeSystemId(
    const QnUuid& /*systemId*/,
    qint64 /*sysIdTime*/,
    Timestamp /*tranLogTime*/,
    impl::SimpleHandlerPtr /*handler*/)
{
    // TODO
    return 0;
}

int MiscManagerStub::markLicenseOverflow(
    bool /*value*/,
    qint64 /*time*/,
    impl::SimpleHandlerPtr /*handler*/)
{
    // TODO
    return 0;
}

int MiscManagerStub::cleanupDatabase(
    bool cleanupDbObjects,
    bool cleanupTransactionLog,
    impl::SimpleHandlerPtr handler)
{
    // TODO
    return 0;
}

int MiscManagerStub::saveMiscParam(
    const ec2::ApiMiscData& /*param*/,
    impl::SimpleHandlerPtr /*handler*/)
{
    // TODO
    return 0;
}

int MiscManagerStub::saveRuntimeInfo(
    const ec2::ApiRuntimeData& /*data*/,
    impl::SimpleHandlerPtr /*handler*/)
{
    // TODO
    return 0;
}

int MiscManagerStub::getMiscParam(
    const QByteArray& /*paramName*/,
    impl::GetMiscParamHandlerPtr /*handler*/)
{
    // TODO
    return 0;
}

} // namespace test
} // namespace ec2
