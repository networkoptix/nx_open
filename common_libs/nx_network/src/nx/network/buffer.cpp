#include "buffer.h"

#include <cstring>

namespace nx {

bool operator==(const std::string& left, const nx::String& right)
{
    if (left.size() != static_cast<size_t>(right.size()))
        return false;
    return memcmp(left.data(), right.constData(), left.size()) == 0;
}

bool operator==(const nx::String& left, const std::string& right)
{
    return right == left;
}

bool operator!=(const std::string& left, const nx::String& right)
{
    return !(left == right);
}

bool operator!=(const nx::String& left, const std::string& right)
{
    return right != left;
}

String operator+(const String& left, const std::string& right)
{
    return left + String(right.c_str());
}

String operator+(const String& left, const QString& right)
{
    return left + right.toUtf8();
}

static int stringLength(const QByteArray& array)
{
    int size = array.size();
    const char* data = array.constData();
    for (int position = 0; position < size; ++position)
        if (data[position] == 0)
            return position;
    return size;
}

Buffer stringToBuffer(const String& string)
{
    return string;
}

String bufferToString(const Buffer& buffer)
{
    return String(buffer.constData(), stringLength(buffer));
}

std::string toStdString(const Buffer& str)
{
    return std::string(str.constData(), str.size());
}

} // namespace nx
