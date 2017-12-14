#include "actions.h"

static int showHelp()
{
    std::cout
        << "Usage: root_tool <command> <args...>" << std::endl
        << "Supported commands:" << std::endl
        << "    mount <url> <directory> [<user> [<password>]]" << std::endl
        << "    unmount <source|directory>" << std::endl
        << "    chown <file_or_directory_path>" << std::endl
        << "    touch <file_path>" << std::endl
        << "    mkdir <directory_path>" << std::endl
        << "    install <deb_package>" << std::endl;

    return 0;
}

static boost::optional<std::string> getOptionalArg(const char**& argv)
{
    if (*argv == nullptr)
        return boost::none;

    std::string value(*argv);
    ++argv;
    return value;
}

static std::string getArg(const char**& argv, const char* error)
{
    if (auto arg = getOptionalArg(argv))
        return *arg;

    throw std::invalid_argument(error);
}

int main(int /*argc*/, const char** argv)
{
    ++argv; //< Binary name.
    try
    {
        std::string command = getArg(argv, "command is required");
        nx::root_tool::setupIds();

        if (command == std::string("-h") || command == std::string("--help"))
            return showHelp();

        if (command == std::string("mount"))
        {
            const auto url = getArg(argv, "<url> is required");
            const auto directory = getArg(argv, "<directory> is required");
            const auto user = getOptionalArg(argv);
            const auto password = getOptionalArg(argv);
            return nx::root_tool::mount(url, directory, user, password);
        }

        if (command == std::string("umount") || command == std::string("unmount"))
            return nx::root_tool::unmount(getArg(argv, "<source> or <directory> is required"));

        if (command == std::string("chown"))
            return nx::root_tool::chengeOwner(getArg(argv, "<file_or_directory_path> is required"));

        if (command == std::string("touch"))
            return nx::root_tool::chengeOwner(getArg(argv, "<file_path> is required"));

        if (command == std::string("mkdir"))
            return nx::root_tool::chengeOwner(getArg(argv, "<directory_path> is required"));

        if (command == std::string("install"))
            return nx::root_tool::install(getArg(argv, "<deb_package> is required"));

        if (command == std::string("ids"))
            return nx::root_tool::showIds();

        throw std::invalid_argument("Unknown command: " + command);

    }
    catch (const std::invalid_argument& exception)
    {
        std::cout << "Error: " << exception.what() << ", try --help" << std::endl;
        return 1;
    }
    catch (const std::exception& exception)
    {
        std::cout << "Error: " << exception.what() << std::endl;
        return 2;
    }
}
