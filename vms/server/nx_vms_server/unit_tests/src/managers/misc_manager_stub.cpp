#include "misc_manager_stub.h"

#include <nx/utils/std/future.h>

namespace ec2 {
namespace test {

MiscManagerStub::MiscManagerStub():
    m_reqIdSequence(0)
{
}

MiscManagerStub::~MiscManagerStub()
{
    pleaseStopSync();
}

int MiscManagerStub::changeSystemId(
    const QnUuid& /*systemId*/,
    qint64 /*sysIdTime*/,
    nx::vms::api::Timestamp /*tranLogTime*/,
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

int MiscManagerStub::saveSystemMergeHistoryRecord(
    const nx::vms::api::SystemMergeHistoryRecord& /*param*/,
    impl::SimpleHandlerPtr /*handler*/)
{
    // TODO
    return 0;
}

int MiscManagerStub::getSystemMergeHistory(impl::GetSystemMergeHistoryHandlerPtr /*handler*/)
{
    // TODO
    return 0;
}

int MiscManagerStub::saveMiscParam(
    const nx::vms::api::MiscData& param,
    impl::SimpleHandlerPtr handler)
{
    const int reqId = ++m_reqIdSequence;

    post(
        [this, reqId, param, handler = std::move(handler)]()
        {
            m_miscParams[param.name] = param.value;
            handler->done(reqId, ErrorCode::ok);
        });

    return reqId;
}

int MiscManagerStub::saveRuntimeInfo(
    const nx::vms::api::RuntimeData& /*data*/,
    impl::SimpleHandlerPtr /*handler*/)
{
    // TODO
    return 0;
}

int MiscManagerStub::getMiscParam(
    const QByteArray& paramName,
    impl::GetMiscParamHandlerPtr handler)
{
    const int reqId = ++m_reqIdSequence;

    post(
        [this, reqId, paramName, handler = std::move(handler)]()
        {
            QByteArray value;
            auto it = m_miscParams.find(paramName);
            if (it != m_miscParams.end())
                handler->done(reqId, ErrorCode::ok, {paramName, it->second});
            else
                handler->done(reqId, ErrorCode::ioError, {});
        });

    return reqId;
}

//-------------------------------------------------------------------------------------------------

WorkAroundMiscDataSaverStub::WorkAroundMiscDataSaverStub(
    MiscManagerStub* miscManager)
    :
    m_miscManager(miscManager)
{
}

ErrorCode WorkAroundMiscDataSaverStub::saveSync(const nx::vms::api::MiscData& data)
{
    nx::utils::promise<ErrorCode> done;
    m_miscManager->AbstractMiscManager::saveMiscParam(
        data,
        this,
        [&done](int /*reqId*/, ErrorCode result)
        {
            done.set_value(result);
        });
    return done.get_future().get();
}

} // namespace test
} // namespace ec2
