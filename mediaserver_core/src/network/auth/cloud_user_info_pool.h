#pragma once

#include <cstdint>
#include <unordered_map>
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
};

using UserNonceToResponseMap = std::unordered_map<UserNonce, nx::Buffer>;

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
    size_t userCount;
};

using TimestampToNonceUserCountMap = std::map<int64_t, NonceUserCount, std::greater<int64_t>>;

bool deserialize(
    const QString& serializedValue,
    int64_t* timestamp,
    nx::Buffer* cloudNonce,
    nx::Buffer* partialResponse);

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
        int64_t timestamp,
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

    bool authenticate(const nx_http::header::Authorization& authHeader) const;
    boost::optional<nx::Buffer> newestMostCommonNonce() const;

private:
    virtual void userInfoChanged(
        int64_t timestamp,
        const nx::Buffer& userName,
        const nx::Buffer& cloudNonce,
        const nx::Buffer& partialResponse) override;

    virtual void userInfoRemoved(const nx::Buffer& userName) override;


private:
    std::unique_ptr<AbstractCloudUserInfoPoolSupplier> m_supplier;
    detail::UserNonceToResponseMap m_userNonceToResponse;
    detail::TimestampToNonceUserCountMap m_timestampToNonceUserCount;
    QnMutex m_mutex;
};

