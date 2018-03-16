#include <iostream>
#include <assert.h>
#include <nx/system_commands.h>

static int showHelp()
{
    std::cout
        << "Usage: root_tool <command> <args...>" << std::endl
        << "Supported commands:" << std::endl
        << " mount <url> <directory> [<user> [<password>]]" << std::endl
        << " unmount <url|directory>" << std::endl
        << " chown <file_or_directory_path>" << std::endl
        << " touch <file_path>" << std::endl
        << " mkdir <directory_path>" << std::endl
        << " install <deb_package>" << std::endl;

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

static int reportErrorAndExit(const std::string& message)
{
    std::cout << message;
    return -1;
}

static int reportArgErrorAndExit(const std::string& command)
{
    if (command == "umount" || command == "unmount")
        return reportErrorAndExit("<source> or <directory> is required");
    else if (command == "chown")
        return reportErrorAndExit("<file_or_directory_path> is required");
    else if (command == "touch")
        return reportErrorAndExit("<file_path> is required");
    else if (command == "mkdir")
        return reportErrorAndExit("<directory_path> is required");
    else if (command == "install")
        return reportErrorAndExit("<deb_package> is required");

    assert(false);
    return -1;
}

static const std::string kMount = "mount";
static const std::string kUMount1 = "umount";
static const std::string kUMount2 = "unmount";
static const std::string kChown = "chown";
static const std::string kTouch = "touch";
static const std::string kMkdir = "mkdir";
static const std::string kInstall = "install";
static const std::string kIds = "ids";

static bool unknownCommand(const std::string& command)
{
    return command != kMount && command != kUMount1 && command != kUMount2 && command != kChown
        && command != kTouch && command != kMkdir && command != kInstall && command != kIds;
}

int main(int /*argc*/, const char** argv)
{
    if (argv == nullptr || *argv == nullptr)
        return reportErrorAndExit("This program is not supposed to be used without argv.");

    ++argv; //< Binary name.
    const auto command = getOptionalArg(argv);
    if (!command)
        return reportErrorAndExit("command is required");

    if (*command == "-h" || *command == "--help")
        return showHelp();

    if (unknownCommand(*command))
        return reportErrorAndExit("Unknown command: " + *command);

    nx::root_tool::SystemCommands commands;
    if (!commands.setupIds())
        return reportErrorAndExit(commands.lastError());

    if (*command == kMount)
    {
        const auto url = getOptionalArg(argv);
        if (!url)
            return reportErrorAndExit("<url> is required");

        const auto directory = getOptionalArg(argv);
        if (!directory)
            return reportErrorAndExit("<directory> is required");

        const auto user = getOptionalArg(argv);
        const auto password = getOptionalArg(argv);

        if (!commands.mount(*url, *directory, user, password))
            return reportErrorAndExit(commands.lastError());

        return 0;
    }

    if (*command == kIds)
    {
        commands.showIds();
        return 0;
    }

    const auto commandArg = getOptionalArg(argv);
    if (!commandArg)
        return reportArgErrorAndExit(*command);

    if (((*command == kUMount1 || *command == kUMount2) && !commands.unmount(*commandArg))
        || (*command == kChown && !commands.changeOwner(*commandArg))
        || (*command == kTouch && !commands.touchFile(*commandArg))
        || (*command == kMkdir && !commands.makeDirectory(*commandArg))
        || (*command == kInstall && !commands.install(*commandArg)))
    {
        return reportErrorAndExit(commands.lastError());
    }

    return 0;
}
