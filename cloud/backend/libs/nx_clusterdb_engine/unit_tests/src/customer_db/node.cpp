#include "node.h"

#include "dao/structure_updater.h"

namespace nx::clusterdb::engine::test {

static constexpr char kApplicationId[] = "customer_db";
// TODO: #ak Remove following constant
static constexpr char kSystemId[] = "review_and_replace_this_value";

CustomerDbNode::CustomerDbNode(int argc, char **argv):
    base_type(kApplicationId, argc, argv)
{
}

const CustomerManager& CustomerDbNode::customerManager() const
{
    return *m_customerManager;
}

CustomerManager& CustomerDbNode::customerManager()
{
    return *m_customerManager;
}

void CustomerDbNode::setup()
{
    dao::StructureUpdater structureUpdater(&sqlQueryExecutor());

    m_customerDao = std::make_unique<dao::CustomerDao>(&sqlQueryExecutor());

    m_customerManager = std::make_unique<CustomerManager>(
        &syncronizationEngine(),
        m_customerDao.get(),
        kSystemId);
}

void CustomerDbNode::teardown()
{
    m_customerManager.reset();

    m_customerDao.reset();
}

} // namespace nx::clusterdb::engine::test
