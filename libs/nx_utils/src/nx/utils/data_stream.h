// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <string_view>

#include <QtCore/QDataStream>

namespace nx::utils {

class NX_UTILS_API DataStream:
    public QDataStream
{
    using base_type = QDataStream;

public:
    using base_type::base_type;

    /**
     * Creates a read-only data stream that reads from buf.
     * NOTE: The contents of buf are not copied.
     */
    DataStream(const std::string_view& buf);
};

} // namespace nx::utils
