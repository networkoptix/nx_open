#pragma once

#include <memory>

#include <nx/clusterdb/engine/service/service.h>

#include "customer_manager.h"
#include "data.h"
#include "dao/customer.h"

namespace nx::clusterdb::engine::test {

/**
 * Test instance of data_sync_engine.
 */
class CustomerDbNode:
    public Service
{
    using base_type = Service;

public:
    CustomerDbNode(int argc, char **argv);

    const CustomerManager& customerManager() const;
    CustomerManager& customerManager();

protected:
    virtual void setup() override;
    virtual void teardown() override;

private:
    std::unique_ptr<CustomerManager> m_customerManager;
    std::unique_ptr<dao::CustomerDao> m_customerDao;
};

} // namespace nx::clusterdb::engine::test
