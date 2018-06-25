#pragma once

#include <map>

#include <nx/network/aio/basic_pollable.h>


#include <nx_ec/ec_api.h>

namespace ec2 {
namespace test {

class MiscManagerStub:
    public AbstractMiscManager,
    public nx::network::aio::BasicPollable
{
public:
    MiscManagerStub();
    ~MiscManagerStub();

protected:
    virtual int changeSystemId(
        const QnUuid& systemId,
        qint64 sysIdTime,
        nx::vms::api::Timestamp tranLogTime,
        impl::SimpleHandlerPtr handler) override;

    virtual int markLicenseOverflow(
        bool value,
        qint64 time,
        impl::SimpleHandlerPtr handler) override;

    virtual int cleanupDatabase(
        bool cleanupDbObjects,
        bool cleanupTransactionLog,
        impl::SimpleHandlerPtr handler) override;

    virtual int saveMiscParam(
        const nx::vms::api::MiscData& param,
        impl::SimpleHandlerPtr handler) override;

    virtual int getMiscParam(
        const QByteArray& paramName,
        impl::GetMiscParamHandlerPtr handler) override;

    virtual int saveRuntimeInfo(
        const nx::vms::api::RuntimeData& data,
        impl::SimpleHandlerPtr handler) override;

    virtual int saveSystemMergeHistoryRecord(
        const nx::vms::api::SystemMergeHistoryRecord& param,
        impl::SimpleHandlerPtr handler) override;

    virtual int getSystemMergeHistory(
        impl::GetSystemMergeHistoryHandlerPtr handler) override;

private:
    std::atomic<int> m_reqIdSequence;
    std::map<QByteArray, QByteArray> m_miscParams;
};

//-------------------------------------------------------------------------------------------------

// TODO: #ak This class should be removed when we can use m_miscManager to save parameters.
class AbstractWorkAroundMiscDataSaver
{
public:
    virtual ~AbstractWorkAroundMiscDataSaver() = default;
    virtual ErrorCode saveSync(const nx::vms::api::MiscData& data) = 0;
};

class WorkAroundMiscDataSaverStub:
    public AbstractWorkAroundMiscDataSaver,
    public QObject
{
public:
    WorkAroundMiscDataSaverStub(MiscManagerStub* miscManager);
    virtual ErrorCode saveSync(const nx::vms::api::MiscData& /*data*/) override;

private:
    MiscManagerStub* m_miscManager;
};

} // namespace test
} // namespace ec2
