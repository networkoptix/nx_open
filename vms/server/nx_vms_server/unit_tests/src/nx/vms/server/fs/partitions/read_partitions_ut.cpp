#include <nx/vms/server/fs/partitions/read_partitions_linux.h>
#include <gtest/gtest.h>
#include <string.h>

namespace nx::vms::server::fs::test {

void assertDecodedStrings(QByteArray source, const QByteArray& desired)
{
    decodeOctalEncodedPath(source.data());
    ASSERT_EQ(0, strcmp(source.constData(), desired.constData()));
}

TEST(decodeOctalEncodedPath, variousCases)
{
    assertDecodedStrings("abcdef", "abcdef");
    assertDecodedStrings("ab\040cdef", "ab cdef");
    assertDecodedStrings("ab\40cdef", "ab\40cdef");
    assertDecodedStrings("abcdef\0", "abcdef\0");
    assertDecodedStrings("ab\040", "ab ");
}

} // namespace nx::vms::server::fs::test
