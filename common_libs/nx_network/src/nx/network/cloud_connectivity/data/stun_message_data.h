/**********************************************************
* Dec 22, 2015
* akolesnikov
***********************************************************/

#ifndef STUN_MESSAGE_DATA_H
#define STUN_MESSAGE_DATA_H

#include <nx/network/buffer.h>


namespace nx {
namespace cc {
namespace api {

class NX_NETWORK_API StunMessageData
{
public:
    nx::String errorText() const;

protected:
    void setErrorText(nx::String text);

private:
    nx::String m_text;
};

}   //api
}   //cc
}   //nx

#endif  //STUN_MESSAGE_DATA_H
