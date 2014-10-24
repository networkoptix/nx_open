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

        bool serialize( UnknownAttribute* unknownAttr , const StringAttributeType& val , int user_type ) 
        {
            unknownAttr->user_type = user_type;
            unknownAttr->value = nx::Buffer(val.c_str());
            return true;
        }
    }

}
