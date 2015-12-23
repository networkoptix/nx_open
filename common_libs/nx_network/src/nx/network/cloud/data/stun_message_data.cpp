/**********************************************************
* Dec 22, 2015
* akolesnikov
***********************************************************/

#include "stun_message_data.h"

namespace nx {
namespace hpm {
namespace api {

nx::String StunMessageData::errorText() const
{
    return m_text;
}

void StunMessageData::setErrorText(nx::String text)
{
    m_text = std::move(text);
}

} // namespace api
} // namespace hpm
} // namespace nx
