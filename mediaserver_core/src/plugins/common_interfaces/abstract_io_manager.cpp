#include "abstract_io_manager.h"

using namespace nx_io_managment;

bool nx_io_managment::isActiveIOPortState(IOPortState state)
{
    return state < IOPortState::nonActive;
}

IOPortState nx_io_managment::fromBoolToIOPortState(bool state)
{
    return state ? IOPortState::active : IOPortState::nonActive;
}

