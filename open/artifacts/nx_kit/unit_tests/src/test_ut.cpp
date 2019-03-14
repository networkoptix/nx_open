/**@file
 * Testing features of test.h.
 */

#include <nx/kit/test.h>

#include <fstream>
#include <string>

namespace nx {
namespace kit {
namespace test {
namespace test {

TEST(test, tempDir)
{
    const char* const tempDir = nx::kit::test::tempDir();
    ASSERT_TRUE(tempDir != nullptr);
    std::cerr << "Got tempDir: [" << tempDir << "]" << std::endl;
}

TEST(test, createFile)
{
    ASSERT_TRUE(nx::kit::test::tempDir() != nullptr);

    // Write some data to a test file, and read it back.

    static const char* const contents = "First line\nLine 2\n";

    const std::string filename = nx::kit::test::tempDir() + std::string("test.txt");

    nx::kit::test::createFile(filename.c_str(), contents);

    std::ifstream inputStream(filename);
    ASSERT_TRUE(inputStream.is_open());
    std::string dataFromFile;
    std::string line;
    while (std::getline(inputStream, line))
        dataFromFile += line + "\n";
    inputStream.close();

    ASSERT_EQ(contents, dataFromFile);
}

} // namespace test
} // namespace test
} // namespace kit
} // namespace nx
