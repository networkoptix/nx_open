// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "data_stream.h"

#include <QtCore/QByteArray>

namespace nx::utils {

DataStream::DataStream(const std::string_view& buf):
    base_type(QByteArray::fromRawData(buf.data(), (int) buf.size()))
{
}

} // namespace nx::utils
