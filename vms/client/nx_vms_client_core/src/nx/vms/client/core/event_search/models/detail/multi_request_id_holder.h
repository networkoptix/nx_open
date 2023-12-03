// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <api/server_rest_connection_fwd.h>

namespace nx::vms::client::core::detail {

class NX_VMS_CLIENT_CORE_API MultiRequestIdHolder
{
public:
    enum class Mode
    {
        fetch,
        dynamic,

        count
    };

    rest::Handle setValue(Mode mode, rest::Handle value);
    void resetValue(Mode mode);
    rest::Handle value(Mode mode);

public:
    std::array<rest::Handle, static_cast<int>(Mode::count)> m_ids;
};

} // namespace nx::vms::client::core::detail


