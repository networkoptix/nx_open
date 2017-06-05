#include <gtest/gtest.h>
#include <managers/persistent_timer_manager.h>

namespace nx {
namespace cdb {
namespace test {

class PersistentTimerManager : public ::testing::Test
{
protected:
    nx::cdb::PersistentTimerManager manager;
};


class ManagerUser: public nx::cdb::AbstractPersistentTimerAcceptor
{
public:
    static const QnUuid userTaskGuid;

    ManagerUser()
    {
        NX_REGISTER_PERSISTENT_TASK(this, userTaskGuid, &ManagerUser::onPersistentTimer);
    }

    virtual void persistentTimerFired(
        PersistentTimerManager* manager,
        const PersistentParamsMap& params) override
    {

    }
};

NX_REGISTER_PERSISTENT_USER(ManagerUser);

const QnUuid ManagerUser::userTaskGuid = QnUuid::fromStringSafe("{EC05F182-9380-48E3-9C76-AD6C10295136}");

}
}
}