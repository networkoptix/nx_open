/**********************************************************
* 20 dec 2013
* a.kolesnikov
***********************************************************/

#include "stun_message.h"


namespace nx_stun
{

    void Message::clear()
    {
        attributes.clear();
    }


    namespace attr
    {
        bool parse( const UnknownAttribute& unknownAttr, StringAttributeType* val )
        {
            val->assign(unknownAttr.value.constData(),unknownAttr.value.size());
            return true;
        }
    }

}
