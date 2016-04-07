/**********************************************************
* mar 29, 2016
* a.kolesnikov
***********************************************************/

#pragma once

#include <iostream>
#include <map>

#include <QString>

#include <nx/network/test_support/socket_test_helper.h>
#include <nx/utils/move_only_func.h>


int runInListenMode(const std::multimap<QString, QString>& args);
void printListenOptions(std::ostream* const outStream);
int printStatsAndWaitForCompletion(
    nx::network::test::ConnectionPool* const connectionPool,
    nx::utils::MoveOnlyFunc<bool()> interruptCondition);
