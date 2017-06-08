#pragma once

#include <cstdint>
#include <unordered_map>
#include <unordered_set>
#include <map>
#include <boost/optional.hpp>
#include <nx/network/http/http_types.h>
#include <nx/network/buffer.h>
#include <nx/utils/thread/mutex.h>
#include <nx/utils/safe_direct_connection.h>
#include <core/resource/resource_fwd.h>
#include <common/common_module_aware.h>

namespace detail {

struct UserNonce
{
    nx::Buffer userName;
    nx::Buffer cloudNonce;

    UserNonce(const nx::Buffer& userName, const nx::Buffer& cloudNonce):
        userName(userName),
        cloudNonce(cloudNonce)
    {}
};

using UserNonceToResponseMap = std::unordered_map<UserNonce, nx::Buffer>;

inline bool operator == (const UserNonce& lhs, const UserNonce& rhs)
{
    return lhs.userName == rhs.userName && lhs.cloudNonce == rhs.cloudNonce;
}

using NameToNoncesMap = std::unordered_map<nx::Buffer, std::unordered_set<nx::Buffer>>;
using NonceToTsMap = std::unordered_map<nx::Buffer, uint64_t>;

}

namespace std {

template<>
struct hash<detail::UserNonce>
{
    size_t operator()(const detail::UserNonce& userNonce) const
    {
        return qHash(userNonce.userName) ^ qHash(userNonce.cloudNonce);
    }
};

}

namespace detail {

struct NonceUserCount
{
    nx::Buffer cloudNonce;
    int userCount;

    NonceUserCount(const nx::Buffer& cloudNonce, int userCount):
        cloudNonce(cloudNonce),
        userCount(userCount)
    {}

    NonceUserCount() = default;
};

using TimestampToNonceUserCountMap = std::map<uint64_t, NonceUserCount>;

bool deserialize(
    const QString& serializedValue,
    std::vector<std::tuple<uint64_t, nx::Buffer, nx::Buffer>>* cloudUserInfoDataVector);

} // namespace detail

class AbstractCloudUserInfoPool;

class AbstractCloudUserInfoPoolSupplier
{
public:
    virtual void setPool(AbstractCloudUserInfoPool* pool) = 0;
    virtual ~AbstractCloudUserInfoPoolSupplier() {}
};

class AbstractCloudUserInfoPool
{
public:
    virtual void userInfoChanged(
        uint64_t timestamp,
        const nx::Buffer& userName,
        const nx::Buffer& cloudNonce,
        const nx::Buffer& partialResponse) = 0;

    virtual void userInfoRemoved(const nx::Buffer& userName) = 0;
    virtual ~AbstractCloudUserInfoPool() {}
};

class CloudUserInfoPoolSupplier:
    public QObject,
    public Qn::EnableSafeDirectConnection,
    public QnCommonModuleAware,
    public AbstractCloudUserInfoPoolSupplier
{
    Q_OBJECT
public:
    virtual void setPool(AbstractCloudUserInfoPool* pool) override;
    CloudUserInfoPoolSupplier(QnCommonModule* commonModule);
    ~CloudUserInfoPoolSupplier();

private:
    void onNewResource(const QnResourcePtr& resource);
    void onRemoveResource(const QnResourcePtr& resource);
    void reportInfoChanged(const QString& userName, const QString& serializedValue);
    void connectToResourcePool();

private:
    AbstractCloudUserInfoPool* m_pool;
};


class CloudUserInfoPool : public AbstractCloudUserInfoPool
{
public:
    CloudUserInfoPool(std::unique_ptr<AbstractCloudUserInfoPoolSupplier> supplier);

    bool authenticate(
        const nx_http::Method::ValueType& method,
        const nx_http::header::Authorization& authHeader) const;
    boost::optional<nx::Buffer> newestMostCommonNonce() const;

private:
    virtual void userInfoChanged(
        uint64_t timestamp,
        const nx::Buffer& userName,
        const nx::Buffer& cloudNonce,
        const nx::Buffer& partialResponse) override;

    virtual void userInfoRemoved(const nx::Buffer& userName) override;

    void decUserCountByNonce(const nx::Buffer& cloudNonce);
    void updateNameToNonces(const nx::Buffer& userName, const nx::Buffer& cloudNonce);
    void updateNonceToTs(const nx::Buffer& cloudNonce, uint64_t timestamp);
    void updateUserNonceToResponse(
        const nx::Buffer& userName,
        const nx::Buffer& cloudNonce,
        const nx::Buffer& partialResponse);
    void updateTsToNonceCount(uint64_t timestamp, const nx::Buffer& cloudNonce);
    void cleanupOldInfo(uint64_t timestamp);
    void cleanupByNonce(const nx::Buffer& cloudNonce);
    boost::optional<nx::Buffer> partialResponseByUserNonce(
        const nx::Buffer& userName,
        const nx::Buffer& cloudNonce) const;

protected:
    std::unique_ptr<AbstractCloudUserInfoPoolSupplier> m_supplier;
    detail::UserNonceToResponseMap m_userNonceToResponse;
    detail::TimestampToNonceUserCountMap m_tsToNonceUserCount;
    detail::NameToNoncesMap m_nameToNonces;
    detail::NonceToTsMap m_nonceToTs;
    int m_userCount = 0;
    mutable QnMutex m_mutex;
};

