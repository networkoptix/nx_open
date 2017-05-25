#pragma once

#include <cstdint>
#include <unordered_map>
#include <map>
#include <boost/optional.hpp>
#include <nx/network/http/http_types.h>
#include <nx/network/buffer.h>

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
struct Hash<detail::UserNonce>
{
    size_t operator()(const detail::UserNonce& userNonce)
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

using TimestampToNonceUserCountMap = std::map<int64_t, NonceUserCount, std::greater<>>;

}

class QnResourcePool;

class CloudUserInfoPool
{
public:
    CloudUserInfoPool();
    ~CloudUserInfoPool();

    bool authenticate(const nx::http::header::Authorization& authHeader) const;
    boost::optional<nx::Buffer> newestMostCommonNonce() const;

private:
    void connectToResourcePool();
    void disconnectFromResourcePool();
    QnResourcePool* resourcePool();


private:
    detail::UserNonceToResponseMap m_userNonceToResponse;
    detail::TimestampToNonceUserCountMap m_timestampToNonceUserCount;
};
