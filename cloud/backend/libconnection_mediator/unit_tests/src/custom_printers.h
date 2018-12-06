#pragma once

#include <iostream>

#include <nx/cloud/db/api/system_data.h>

void PrintTo(const boost::optional<nx::cloud::db::api::SystemData>& val, ::std::ostream* os);
