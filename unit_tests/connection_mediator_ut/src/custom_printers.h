#pragma once

#include <iostream>

#include <nx/cloud/cdb/api/system_data.h>

void PrintTo(const boost::optional<nx::cdb::api::SystemData>& val, ::std::ostream* os);
