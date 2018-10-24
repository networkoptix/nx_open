#include "vibrator.h"

#if !defined(Q_OS_IOS) && !defined(Q_OS_ANDROID)
namespace {

void vibrateInternal(int milliseconds)
{
    qWarning() << "--- Vibrating for " << milliseconds << "ms";
}

} // namespace
#endif

#if defined(Q_OS_ANDROID)
namespace {

void vibrateInternal(int milliseconds)
{
    qWarning() << "--- Vibrating for " << milliseconds << "ms";
}

} // namespace
#endif

#if defined(Q_OS_IOS)
namespace {

void vibrateInternal(int milliseconds)
{
    qWarning() << "--- Vibrating for " << milliseconds << "ms";
}

} // namespace
#endif

namespace nx::mobile_client::helpers
{

void vibrate(int milliseconds)
{
    vibrateInternal(milliseconds);
}

} // namespace nx::mobile_client::helpers
