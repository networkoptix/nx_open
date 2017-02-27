#pragma once

#include <iostream>

#include <cdb/system_data.h>

void PrintTo(const boost::optional<nx::cdb::api::SystemData>& val, ::std::ostream* os);
