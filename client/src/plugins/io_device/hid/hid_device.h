#pragma once

namespace nx {
namespace hid {

class AbstractHidDevice {

public:
    AbstractHidDevice();
    virtual ~AbstractHidDevice();

private:
    void setOnDisconnectedHandler(std::function<void()> handler);
    void setOnStateChangedHandler(std::function<void()> handler);

    std::vector<> getButtons();
    void getAxes();
    void getLeds();
};

} // namespace hid
} // namespace nx