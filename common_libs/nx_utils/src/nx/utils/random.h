/**********************************************************
* Apr 27, 2016
* akolesnikov
***********************************************************/

#pragma once

#include <cstdint>
#include <cstdlib>

#include <QByteArray>


namespace nx {
namespace utils {

/** Uses uniform_int_distribution.
    @return \a false if could not generate random data.

    \note Can return \a false since it may use /dev/urandom on linux 
        and access to device may result in error
*/
NX_UTILS_API bool generateRandomData(std::int8_t* data, std::size_t count);

/** Just calls upper function */
NX_UTILS_API QByteArray generateRandomData(std::size_t count, bool* ok = nullptr);

}   // namespace nx
}   // namespace utils
