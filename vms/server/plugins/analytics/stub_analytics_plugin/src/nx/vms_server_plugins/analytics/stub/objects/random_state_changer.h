#pragma once

#include <random>

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace stub {

class RandomStateChanger
{
public:
    RandomStateChanger(int randModulus = 7, int maxNumberOfStateChanges = 4) :
        m_randModulus(randModulus),
        m_maxStateChanges(maxNumberOfStateChanges)
    {
    }

    void update()
    {
        m_stateChanged = m_maxStateChanges >= 0
            ? rand() % m_randModulus == 0
            : false;

        if (m_stateChanged)
            --m_maxStateChanges;
    }

    bool stateChanged() const
    {
        return m_stateChanged;
    }

private:
    int m_randModulus = 7;
    int m_maxStateChanges = 4;
    bool m_stateChanged = false;
};

} // namespace stub
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx