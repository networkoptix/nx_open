#pragma once

#include <map>

#include <nx/network/aio/basic_pollable.h>

#include <nx_ec/ec_api.h>

#include <managers/time_manager.h>

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
        Timestamp tranLogTime,
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
        const ec2::ApiMiscData& param,
        impl::SimpleHandlerPtr handler) override;

    virtual int getMiscParam(
        const QByteArray& paramName,
        impl::GetMiscParamHandlerPtr handler) override;

    virtual int saveRuntimeInfo(
        const ec2::ApiRuntimeData& data,
        impl::SimpleHandlerPtr handler) override;

    virtual int saveSystemMergeHistoryRecord(
        const ApiSystemMergeHistoryRecord& param,
        impl::SimpleHandlerPtr handler) override;

    virtual int getSystemMergeHistory(
        impl::GetSystemMergeHistoryHandlerPtr handler) override;

private:
    std::atomic<int> m_reqIdSequence;
    std::map<QByteArray, QByteArray> m_miscParams;
};

//-------------------------------------------------------------------------------------------------

class WorkAroundMiscDataSaverStub:
    public AbstractWorkAroundMiscDataSaver,
    public QObject
{
public:
    WorkAroundMiscDataSaverStub(MiscManagerStub* miscManager);

    virtual ErrorCode saveSync(const ApiMiscData& /*data*/) override;

private:
    MiscManagerStub* m_miscManager;
};

} // namespace test
} // namespace ec2
