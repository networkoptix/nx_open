#pragma once

#include <string>
#include <unordered_map>
#include <nx/utils/uuid.h>
#include <nx/utils/singleton.h>

namespace nx {
namespace cdb {

class PersistentTimerManager;

using PersistentParamsMap =
    std::unordered_map<std::string, std::unordered_map<std::string, std::string>>;

class AbstractPersistentTimerAcceptor
{
public:
    virtual void persistentTimerFired(
        PersistentTimerManager* manager,
        PersistentParamsMap& params) = 0;

    virtual ~AbstractPersistentTimerAcceptor() {}
};

namespace detail {

using GuidToHandlerMap = std::unordered_map<QnUuid, std::function<void(AbstractPersistentTimerAcceptor*, PersistentTimerManager*, const PersistentParamsMap&)>>;

}

class PersistentTimerManager: public Singleton<PersistentTimerManager>
{
public:
};

}
}