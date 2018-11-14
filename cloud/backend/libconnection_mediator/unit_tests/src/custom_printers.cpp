#include "custom_printers.h"

#include <iostream>
#include <string>

#include <nx/cloud/cdb/api/system_data.h>

void PrintTo(const boost::optional<nx::cloud::db::api::SystemData>& val, ::std::ostream* os) {
    if( val )
        *os << "SystemData" << &val.get();
    else
        *os << "boost::none";
}
