#include <future>
#include <iostream>
#include <tuple>

#include <nx/network/socket_global.h>
#include <nx/utils/argument_parser.h>
#include <nx/utils/log/log_initializer.h>
#include <nx/utils/std/filesystem.h>

#include <nx/cloud/aws/api_client.h>

void printHelp(const char* name);
int upload(const nx::utils::ArgumentParser& arguments);
int download(const nx::utils::ArgumentParser& arguments);

int main(int argc, const char* argv[])
{
    nx::utils::ArgumentParser arguments(argc - 1, argv + 1);

    nx::utils::log::initializeGlobally(arguments);

    if (arguments.contains("h") || arguments.contains("help"))
    {
        printHelp(argv[0]);
        return 0;
    }

    nx::network::SocketGlobalsHolder socketGlobalsHolder;

    if (arguments.contains("u"))
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
        "   --user=user:password" << std::endl <<
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
    std::string url;

    std::string user;
    std::string password;

    std::string region;

    static std::tuple<CommonSettings, bool /*result*/> read(
        const nx::utils::ArgumentParser& arguments)
    {
        CommonSettings settings;

        if (auto credentialsStr = arguments.get("user"))
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

        const auto positionalArgs = arguments.getPositionalArgs();
        if (positionalArgs.empty())
        {
            std::cerr << "Missing required argument URL. See -h" << std::endl;
            return std::make_tuple(settings, false);
        }

        settings.url = positionalArgs.front().toStdString();

        return std::make_tuple(std::move(settings), true);
    }
};

std::unique_ptr<nx::cloud::aws::ApiClient>
    prepareApiClient(const CommonSettings& settings)
{
    return std::make_unique<nx::cloud::aws::ApiClient>(
        "cmd_line_client",
        settings.region,
        nx::utils::Url(settings.url),
        nx::network::http::Credentials(
            settings.user.c_str(),
            nx::network::http::PasswordAuthToken(settings.password.c_str())));
}

//-------------------------------------------------------------------------------------------------

struct UploadSettings
{
    CommonSettings common;
    std::string inputFilePath;

    static std::tuple<UploadSettings, bool /*result*/> read(
        const nx::utils::ArgumentParser& arguments)
    {
        UploadSettings settings;

        bool result = false;
        std::tie(settings.common, result) = CommonSettings::read(arguments);
        if (!result)
            return std::make_tuple(settings, false);

        if (auto arg = arguments.get("i"))
            settings.inputFilePath = arg->toStdString();

        return std::make_tuple(settings, true);
    }
};

int upload(const nx::utils::ArgumentParser& arguments)
{
    using namespace nx::cloud::aws;

    const auto [settings, result] = UploadSettings::read(arguments);
    if (!result)
        return 1;

    auto client = prepareApiClient(settings.common);

    std::ifstream contentFile(settings.inputFilePath, std::ios_base::in | std::ios_base::binary);
    if (!contentFile.is_open())
        return false;
    std::stringstream contents;
    contents << contentFile.rdbuf();

    std::promise<Result> done;
    client->uploadFile(
        std::filesystem::path(settings.inputFilePath).filename().string(),
        nx::Buffer::fromStdString(contents.str()),
        [&done](auto result) { done.set_value(result); });

    const auto uploadResult = done.get_future().get();
    if (!uploadResult.ok())
    {
        std::cerr << "Error: " << uploadResult.text() << std::endl;
        return 2;
    }

    return 0;
}

//-------------------------------------------------------------------------------------------------

struct DownloadSettings
{
    CommonSettings common;
    std::string outputFilePath;

    static std::tuple<DownloadSettings, bool /*result*/> read(
        const nx::utils::ArgumentParser& arguments)
    {
        DownloadSettings settings;

        bool result = false;
        std::tie(settings.common, result) = CommonSettings::read(arguments);
        if (!result)
            return std::make_tuple(settings, false);

        if (auto arg = arguments.get("o"))
            settings.outputFilePath = arg->toStdString();

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
    using namespace nx::cloud::aws;

    const auto [settings, result] = DownloadSettings::read(arguments);
    if (!result)
        return 1;

    auto client = prepareApiClient(settings.common);

    std::promise<std::tuple<Result, nx::Buffer>> done;
    client->downloadFile(
        "",
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
