#pragma once

#include <nx/utils/test_support/test_with_temporary_directory.h>

namespace nx {
namespace cloud {
namespace relay {
namespace test {

class RelayTestSetup:
    public utils::test::TestWithTemporaryDirectory
{
public:
    RelayTestSetup();
};

} // namespace test
} // namespace relay
} // namespace cloud
} // namespace nx
