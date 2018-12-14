#pragma once

#include <functional>
#include <vector>

/**
 * Abstract class providing interface to stop doing anything without object destruction.
 */
class NX_UTILS_API QnStoppable
{
public:
    virtual ~QnStoppable() = default;

    virtual void pleaseStop() = 0;
};
