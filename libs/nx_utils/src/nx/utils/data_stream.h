// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <string_view>

#include <QtCore/QDataStream>

namespace nx::utils {

class DataStream: public QDataStream
{
    using base_type = QDataStream;

public:
    using base_type::base_type;

    /**
     * Creates a read-only data stream that reads from buf.
     * NOTE: The contents of buf are not copied.
     */
    // Exported per-member rather than on the class: dllexport of the class force-emits the
    // inherited QDataStream constructors, which crashes clang-cl 23 in codegen.
    NX_UTILS_API DataStream(std::string_view buf);
};

} // namespace nx::utils
