#pragma once

namespace nx::client::core {

class TimeConstants
{
public:
    static const int kMinYear;
    static const int kMaxYear;

    static const int kMinMonth;
    static const int kMaxMonth;

    static const int kMinDisplayOffset;
    static const int kMaxDisplayOffset;

private:
    TimeConstants() = default;
};

} // namespace nx::client::core
