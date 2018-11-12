#pragma once

#include <cstdint>
#include <vector>

#include <boost/optional.hpp>

#include <nx/network/http/http_types.h>
#include <nx/network/buffer.h>
#include <nx/utils/thread/mutex.h>
#include <nx/utils/safe_direct_connection.h>

#include <core/resource/resource_fwd.h>
#include <common/common_globals.h>

namespace nx {
namespace cdb {
namespace api {

class AuthInfo;

} // namespace api
} // namespace cdb
} // namespace nx

class QnResourcePool;

namespace nx {
namespace vms {
namespace cloud_integration {

namespace detail {

struct CloudUserInfoRecord
{
    uint64_t timestamp;
    nx::Buffer userName;
    nx::Buffer nonce;
    nx::Buffer intermediateResponse;

    CloudUserInfoRecord(
        uint64_t timestamp,
        const nx::Buffer& userName,
        const nx::Buffer& nonce,
        const nx::Buffer& intermediateResponse)
        :
        timestamp(timestamp),
        userName(userName),
        nonce(nonce),
        intermediateResponse(intermediateResponse)
    {}
};

using CloudUserRecordInfoRecordList = std::vector<CloudUserInfoRecord>;

} // namespace detail

class AbstractCloudUserInfoPool;

// TODO: #ak Break dependency loop between AbstractCloudUserInfoPoolSupplier and AbstractCloudUserInfoPool.

class AbstractCloudUserInfoPoolSupplier
{
public:
    virtual void setPool(AbstractCloudUserInfoPool* pool) = 0;
    virtual ~AbstractCloudUserInfoPoolSupplier() {}
};

class AbstractCloudUserInfoPool
{
public:
    virtual ~AbstractCloudUserInfoPool() = default;

    virtual boost::optional<nx::Buffer> newestMostCommonNonce() const = 0;
    // TODO: #ak Likely, this method has to be removed after corresponding fix in CdbNonceFetcher.
    virtual void clear() = 0;

    // TODO: #ak Looks like dependency inversion make be of some use here. 
    // It will break dependency loop also.
    virtual void userInfoChanged(
        const nx::Buffer& userName,
        const nx::cdb::api::AuthInfo& authInfo) = 0;

    virtual void userInfoRemoved(const nx::Buffer& userName) = 0;
};

class CloudUserInfoPoolSupplier:
    public QObject,
    public Qn::EnableSafeDirectConnection,
    public AbstractCloudUserInfoPoolSupplier
{
    Q_OBJECT
public:
    CloudUserInfoPoolSupplier(QnResourcePool* resourcePool);
    ~CloudUserInfoPoolSupplier();

    virtual void setPool(AbstractCloudUserInfoPool* pool) override;

private:
    void onNewResource(const QnResourcePtr& resource);
    void onRemoveResource(const QnResourcePtr& resource);
    void reportInfoChanged(const QString& userName, const QString& serializedValue);
    void connectToResourcePool();

private:
    QnResourcePool* m_resourcePool;
    AbstractCloudUserInfoPool* m_pool;
};

class CloudUserInfoPool:
    public AbstractCloudUserInfoPool
{
public:
    CloudUserInfoPool(std::unique_ptr<AbstractCloudUserInfoPoolSupplier> supplier);

    virtual boost::optional<nx::Buffer> newestMostCommonNonce() const override;
    virtual void clear() override;

    Qn::AuthResult authenticate(
        const nx::network::http::Method::ValueType& method,
        const nx::network::http::header::Authorization& authHeader) const;

private:
    virtual void userInfoChanged(
        const nx::Buffer& userName,
        const nx::cdb::api::AuthInfo& authInfo) override;

    virtual void userInfoRemoved(const nx::Buffer& userName) override;
    void updateNonce();
    void removeInfoForUser(const nx::Buffer& userName);
    void logNonceUpdates(
        const std::map<nx::Buffer, int>& nonceToCount,
        const std::map<nx::Buffer, uint64_t>& nonceToMaxTs);
    boost::optional<nx::Buffer> intermediateResponseByUserNonce(
        const nx::Buffer& userName,
        const nx::Buffer& cloudNonce) const;

protected:
    std::unique_ptr<AbstractCloudUserInfoPoolSupplier> m_supplier;
    detail::CloudUserRecordInfoRecordList m_cloudUserInfoRecordList;
    nx::Buffer m_nonce;
    mutable QnMutex m_mutex;
};

} // namespace cloud_integration
} // namespace vms
} // namespace nx
