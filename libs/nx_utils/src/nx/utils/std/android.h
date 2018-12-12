#pragma once

/**@file
 * Some C++ features missing in Android NDK are defined here.
 */

#if defined(ANDROID) || defined(__ANDROID__)

#include <sstream>

namespace std {

template<typename T>
string to_string(const T& value)
{
    ostringstream os;
    os << value;
    return os.str();
}

int stoi(string str)
{
   stringstream ss(str);
   int n;
   ss << str;
   ss >> n;
   return n;
}

} // namespace std

#endif // defined(ANDROID) || defined(__ANDROID__)
