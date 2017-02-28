#include "custom_printers.h"

#include <iostream>
#include <string>

#include <cdb/system_data.h>

void PrintTo(const boost::optional<nx::cdb::api::SystemData>& val, ::std::ostream* os) {
    if( val )
        *os << "SystemData" << &val.get();
    else
        *os << "boost::none";
}
