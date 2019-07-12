#include <future>
#include <iostream>
#include <tuple>

#include <nx/network/socket_global.h>
#include <nx/utils/argument_parser.h>
#include <nx/utils/log/log_initializer.h>

#include <nx/cloud/storage/client/aws_s3/api_client.h>

void printHelp(const char* name);
int upload(const nx::utils::ArgumentParser& arguments);
int download(const nx::utils::ArgumentParser& arguments);

int main(int argc, const char* argv[])
{
    nx::utils::ArgumentParser arguments(argc - 1, argv + 1);

    nx::utils::log::initializeGlobally(arguments);

    if (arguments.contains("-h") || arguments.contains("--help"))
    {
        printHelp(argv[0]);
        return 0;
    }

    nx::network::SocketGlobalsHolder socketGlobalsHolder;

    if (arguments.contains("-u"))
        return upload(arguments);
    else
        return download(arguments);

    return 0;
}

//-------------------------------------------------------------------------------------------------

void printHelp(const char* name)
{
    std::cout <<
        "Provides some operation with AWS S3 bucket." << std::endl <<
        "Common parameters: " << std::endl <<
        "   [URL] should be s3://BucketName.s3.amazonaws.com/ObjectName" << std::endl <<
        "   -u user:password" << std::endl <<
        "   -r aws_region (e.g., us-west-1)" << std::endl <<
        std::endl <<
        "Usage examples: " << std::endl <<
        "   To download and print to STDOUT use \"" << name << " [URL]\"." << std::endl <<
        "   To download and save to file use \"" << name << " [URL] -O file\"." << std::endl <<
        "   To upload file: \"" << name << " -u file [URL]\"" <<
        std::endl <<
        std::endl;
}

//-------------------------------------------------------------------------------------------------

struct CommonSettings
{
    std::string user;
    std::string password;

    std::string region;

    static std::tuple<CommonSettings, bool /*result*/> read(
        const nx::utils::ArgumentParser& arguments)
    {
        CommonSettings settings;

        if (auto credentialsStr = arguments.get("u"))
        {
            const auto tokens = credentialsStr->split(":");
            if (tokens.empty())
            {
                std::cerr << "Credentials cannot be empty" << std::endl;
                return std::make_tuple(settings, false);
            }

            settings.user = tokens[0].toStdString();
            if (tokens.size() > 1)
                settings.password = tokens[1].toStdString();
        }

        auto region = arguments.get("r");
        if (!region)
        {
            std::cerr << "AWS region MUST be specified" << std::endl;
            return std::make_tuple(settings, false);
        }
        settings.region = region->toStdString();

        return std::make_tuple(std::move(settings), true);
    }
};

//-------------------------------------------------------------------------------------------------

int upload(const nx::utils::ArgumentParser& /*arguments*/)
{
    // TODO
    return 1;
}

//-------------------------------------------------------------------------------------------------

struct DownloadSettings
{
    CommonSettings common;
    std::string outputFilePath;
    std::string url;

    static std::tuple<DownloadSettings, bool /*result*/> read(
        const nx::utils::ArgumentParser& arguments)
    {
        DownloadSettings settings;

        const auto& [commonSettings, result] = CommonSettings::read(arguments);
        if (!result)
            return std::make_tuple(settings, false);
        settings.common = commonSettings;

        if (auto arg = arguments.get("o"))
            settings.outputFilePath = arg->toStdString();

        const auto unnamedArgs = arguments.getPositionalArgs();
        if (unnamedArgs.empty())
        {
            std::cerr << "Missing required argument URL. See -h" << std::endl;
            return std::make_tuple(settings, false);
        }

        settings.url = unnamedArgs.front().toStdString();

        return std::make_tuple(settings, true);
    }
};

int writeTo(const std::string& path, const nx::Buffer& data)
{
    if (!path.empty())
    {
        // TODO: Saving to file.
    }
    else
    {
        std::cout << std::string_view(data.data(), data.size()) << std::endl;
    }

    return 0;
}

int download(const nx::utils::ArgumentParser& arguments)
{
    using namespace nx::cloud::storage::client::aws_s3;

    const auto [settings, result] = DownloadSettings::read(arguments);
    if (!result)
        return 1;

    nx::utils::Url baseUrl(settings.url);
    const auto path = baseUrl.path().toStdString();
    baseUrl.setPath("");

    ApiClient client(
        "cmd_line_client",
        settings.common.region,
        baseUrl,
        nx::network::http::Credentials(
            settings.common.user.c_str(),
            nx::network::http::PasswordAuthToken(settings.common.password.c_str())));

    std::promise<std::tuple<Result, nx::Buffer>> done;
    client.downloadFile(
        path,
        [&done](auto result, auto fileContents)
        {
            done.set_value(std::make_tuple(result, std::move(fileContents)));
        });

    const auto [downloadResult, fileContents] = done.get_future().get();
    if (!downloadResult.ok())
    {
        std::cerr << "Error: " << downloadResult.text() << std::endl;
        return 2;
    }

    return writeTo(settings.outputFilePath, fileContents);
}
