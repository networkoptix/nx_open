#pragma once

namespace nx {
namespace cloud {
namespace relay {

namespace conf { class Settings; }
class Controller;
class Model;

class View
{
public:
    View(
        const conf::Settings& settings,
        const Model& model,
        Controller* controller);

    void start();

    // TODO: Bring up Http server here.
};

} // namespace relay
} // namespace cloud
} // namespace nx
