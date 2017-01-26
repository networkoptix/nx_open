#pragma once

#include <iostream>
#include <map>

#include <QString>

#include <nx/network/test_support/socket_test_helper.h>
#include <nx/utils/argument_parser.h>
#include <nx/utils/move_only_func.h>

namespace nx {
namespace cctu {

void printListenOptions(std::ostream* const outStream);
int runInListenMode(const nx::utils::ArgumentParser& args);

int printStatsAndWaitForCompletion(
    nx::network::test::ConnectionPool* const connectionPool,
    nx::utils::MoveOnlyFunc<bool()> interruptCondition);

String makeServerName(const QString& prefix, size_t number);
void limitStringList(QStringList* list);

} // namespace cctu
} // namespace nx
