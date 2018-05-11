#include <iostream>
#include <assert.h>
#include <string>
#include <nx/system_commands.h>
#include "command_factory.h"
#include "command.h"

static boost::optional<std::string> getOptionalArg(const char**& argv)
{
    if (*argv == nullptr)
        return boost::none;

    std::string value(*argv);
    ++argv;
    return value;
}

void registerCommands(CommandsFactory& factory, nx::SystemCommands* systemCommands)
{
    using namespace std::placeholders;

    auto oneArgAction =
        [systemCommands](auto action)
        {
            return
                [systemCommands, action](const char** argv)
                {
                    auto commandArg = getOptionalArg(argv);
                    if (!commandArg)
                        return Result::invalidArg;

                    return action(*commandArg) ? Result::ok : Result::execFailed;
                };
        };

    factory.reg({"chown"}, {"path"},
        oneArgAction(std::bind(&nx::SystemCommands::changeOwner, systemCommands, _1)));
    factory.reg({"mkdir"}, {"path"},
        oneArgAction(std::bind(&nx::SystemCommands::makeDirectory, systemCommands, _1)));
    factory.reg({"rm"}, {"path"},
        oneArgAction(std::bind(&nx::SystemCommands::removePath, systemCommands, _1)));
    factory.reg({"install"}, {"deb_package"},
        oneArgAction(std::bind(&nx::SystemCommands::install, systemCommands, _1)));

    factory.reg(
        {"mount"}, {"url", "path", "opt_user", "opt_password"},
        [systemCommands](const char** argv)
        {
            const auto socketPostfix = getOptionalArg(argv);
            if (!socketPostfix)
                return Result::invalidArg;

            const auto url = getOptionalArg(argv);
            if (!url)
                return Result::invalidArg;

            const auto directory = getOptionalArg(argv);
            if (!directory)
                return Result::invalidArg;

            const auto user = getOptionalArg(argv);
            const auto password = getOptionalArg(argv);

            return systemCommands->mount(
                *url, *directory, user, password, /*usePipe*/ true, std::stol(*socketPostfix))
                    == nx::SystemCommands::MountCode::ok ? Result::ok : Result::execFailed;
        });

    factory.reg({"mv"}, {"source_path", "target_path"},
        [systemCommands](const char** argv)
        {
            const auto source = getOptionalArg(argv);
            if (!source)
                return Result::invalidArg;

            const auto target = getOptionalArg(argv);
            if (!target)
                return Result::invalidArg;

            return systemCommands->rename(*source, *target) ? Result::ok : Result::execFailed;
        });

    factory.reg({"open"}, {"file_path", "mode"},
        [systemCommands](const char** argv)
        {
            const auto socketPostfix = getOptionalArg(argv);
            if (!socketPostfix)
                return Result::invalidArg;

            const auto path = getOptionalArg(argv);
            if (!path)
                return Result::invalidArg;

            const auto mode = getOptionalArg(argv);
            if (!mode)
                return Result::invalidArg;

            return systemCommands->open(
                *path, std::stoi(*mode), /*usePipe*/ true, std::stol(*socketPostfix)) == -1
                    ? Result::execFailed : Result::ok;
        });

    factory.reg({"freeSpace"}, {"path"},
        [systemCommands](const char** argv)
        {
            const auto socketPostfix = getOptionalArg(argv);
            if (!socketPostfix)
                return Result::invalidArg;

            const auto path = getOptionalArg(argv);
            if (!path)
                return Result::invalidArg;

            return systemCommands->freeSpace(*path, /*usePipe*/ true, std::stol(*socketPostfix)) == -1
                ? Result::execFailed : Result::ok;
        });

    factory.reg({"totalSpace"}, {"path"},
        [systemCommands](const char** argv)
        {
            const auto socketPostfix = getOptionalArg(argv);
            if (!socketPostfix)
                return Result::invalidArg;

            const auto path = getOptionalArg(argv);
            if (!path)
                return Result::invalidArg;

            return systemCommands->totalSpace(*path, /*usePipe*/ true, std::stol(*socketPostfix)) == -1
                ? Result::execFailed : Result::ok;
        });

    factory.reg({"exists"}, {"path"},
        [systemCommands](const char** argv)
        {
            const auto socketPostfix = getOptionalArg(argv);
            if (!socketPostfix)
                return Result::invalidArg;

            const auto path = getOptionalArg(argv);
            if (!path)
                return Result::invalidArg;

            systemCommands->isPathExists(*path, /*usePipe*/ true, std::stol(*socketPostfix));

            return Result::ok;
        });

    factory.reg({"list"}, {"path"},
        [systemCommands](const char** argv)
        {
            const auto socketPostfix = getOptionalArg(argv);
            if (!socketPostfix)
                return Result::invalidArg;

            const auto path = getOptionalArg(argv);
            if (!path)
                return Result::invalidArg;

            systemCommands->serializedFileList(*path, /*usePipe*/ true, std::stol(*socketPostfix));

            return Result::ok;
        });

    factory.reg({"devicePath"}, {"path"},
        [systemCommands](const char** argv)
        {
            const auto socketPostfix = getOptionalArg(argv);
            if (!socketPostfix)
                return Result::invalidArg;

            const auto path = getOptionalArg(argv);
            if (!path)
                return Result::invalidArg;

            systemCommands->devicePath(*path, /*usePipe*/ true, std::stol(*socketPostfix));

            return Result::ok;
        });

    factory.reg({"size"}, {"path"},
        [systemCommands](const char** argv)
        {
            const auto socketPostfix = getOptionalArg(argv);
            if (!socketPostfix)
                return Result::invalidArg;

            const auto path = getOptionalArg(argv);
            if (!path)
                return Result::invalidArg;

            return systemCommands->fileSize(*path, /*usePipe*/ true, std::stol(*socketPostfix)) == -1
                ? Result::execFailed : Result::ok;
        });

    factory.reg({"kill"}, {"pid"},
        [systemCommands](const char** argv)
        {
            const auto pid = getOptionalArg(argv);
            if (!pid)
                return Result::invalidArg;

            return systemCommands->kill(std::stoi(*pid)) ? Result::ok : Result::execFailed;
        });

    factory.reg({"dmiInfo"}, {},
        [systemCommands](const char** argv)
        {
            const auto socketPostfix = getOptionalArg(argv);
            if (!socketPostfix)
                return Result::invalidArg;

            return systemCommands->serializedDmiInfo(true, std::stol(*socketPostfix)).empty()
                ? Result::execFailed : Result::ok;
        });

    factory.reg({"umount", "unmount"}, {"path"},
        [systemCommands](const char** argv)
        {
            const auto socketPostfix = getOptionalArg(argv);
            if (!socketPostfix)
                return Result::invalidArg;

            const auto path = getOptionalArg(argv);
            if (!path)
                return Result::invalidArg;

            return systemCommands->unmount(*path, /*usePipe*/ true, std::stol(*socketPostfix)) ==
                nx::SystemCommands::UnmountCode::ok ? Result::ok : Result::execFailed;
        });

    factory.reg({"help"}, {},
        [&factory](const char** /*argv*/)
        {
            std::cout << factory.help() << std::endl;
            return Result::ok;
        });

    factory.reg({"ids"}, {},
        [systemCommands](const char** /*argv*/)
        {
            systemCommands->showIds();
            return Result::ok;
        });
}

int main(int /*argc*/, const char** argv)
{
    CommandsFactory commandsFactory;
    nx::SystemCommands systemCommands;

    if (!systemCommands.setupIds())
    {
        std::cout << systemCommands.lastError() << std::endl;
        return -1;
    }

    registerCommands(commandsFactory, &systemCommands);

    if (auto command = commandsFactory.get(&argv))
    {
        switch (command->exec(argv))
        {
            case Result::execFailed:
                std::cout << systemCommands.lastError() << std::endl;
                return -1;
            case Result::invalidArg:
                return -1;
            case Result::ok:
                return 0;
        }
    }

    return -1;
}
