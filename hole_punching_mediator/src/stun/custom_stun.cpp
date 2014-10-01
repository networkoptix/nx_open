#include "custom_stun.h"

namespace nx_hpm {
namespace StunParameters {

bool SystemName::parse( const nx_stun::attr::UnknownAttribute& attr ) {
    Q_ASSERT(attr.user_type == static_cast<int>(systemName));
    // An UTF8 encoded string and I need to decode it into std::string 
    system_name = QString::fromUtf8(attr.value).toStdString();
    return !system_name.empty();
}

bool SystemName::serialize( nx_stun::attr::UnknownAttribute* attr ) {
    // Serialize the SystemName into a UnknownAttribute here
    attr->user_type = static_cast<int>(systemName);
    attr->value = QString::fromStdString(system_name).toUtf8();
    attr->length = attr->value.size();
    return true;
}

}
}
