#pragma once

#include <nx_ec/ec_api.h>

namespace ec2 {
namespace test {

class MiscManagerStub:
    public AbstractMiscManager
{
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

    virtual int saveRuntimeInfo(
        const ec2::ApiRuntimeData& data,
        impl::SimpleHandlerPtr handler) override;

    virtual int getMiscParam(
        const QByteArray& paramName,
        impl::GetMiscParamHandlerPtr handler) override;
};

} // namespace test
} // namespace ec2
