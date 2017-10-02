#pragma once

#include <cstring>
#include <iterator>
#include <vector>

namespace nx {
namespace utils {
namespace test {

class NX_UTILS_API ProgramArguments
{
public:
    ~ProgramArguments();

    void addArg(const char* arg);
    void addArg(const char* arg, const char* value);
    void clearArgs();
    std::vector<char*>& args();

    int argc() const;
    char** argv();

private:
    std::vector<char*> m_args;
};

} // namespace test
} // namespace utils
} // namespace nx
