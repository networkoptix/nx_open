#pragma once

#include <iostream>
#include <map>

#include <QString>

#include <nx/network/test_support/socket_test_helper.h>
#include <nx/utils/move_only_func.h>

namespace nx {
namespace cctu {

void printListenOptions(std::ostream* const outStream);
int runInListenMode(const std::multimap<QString, QString>& args);

int printStatsAndWaitForCompletion(
    nx::network::test::ConnectionPool* const connectionPool,
    nx::utils::MoveOnlyFunc<bool()> interruptCondition);

} // namespace cctu
} // namespace nx
