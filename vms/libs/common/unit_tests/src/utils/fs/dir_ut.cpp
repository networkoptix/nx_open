#include <utils/fs/dir.h>
#include <gtest/gtest.h>
#include <string.h>


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
