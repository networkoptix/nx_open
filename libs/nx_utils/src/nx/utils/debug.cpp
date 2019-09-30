#include "debug.h"

namespace nx::utils::debug {

static OnAboutToBlockHandler onAboutToBlockHandler;

OnAboutToBlockHandler setOnAboutToBlockHandler(
    OnAboutToBlockHandler handler)
{
    return std::exchange(onAboutToBlockHandler, std::move(handler));
}

void onAboutToBlock()
{
    if (onAboutToBlockHandler)
        onAboutToBlockHandler();
}

} // namespace nx::utils::debug
