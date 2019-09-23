#include <future>
#include <iostream>
#include <tuple>

#include <nx/network/socket_global.h>
#include <nx/utils/argument_parser.h>
#include <nx/utils/log/log_initializer.h>
#include <nx/utils/std/filesystem.h>

#include <nx/cloud/aws/s3/api_client.h>
#include <nx/cloud/aws/sts/api_client.h>

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
        "   To upload file: \"" << name << " -u -i file [URL]\"" <<
        "   To delete file: \"" << name << " -d [URL]\"" << std::endl <<
        std::endl <<
        "STS calls: " << std::endl <<
        "   --sts-assume-role" << std::endl <<
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

template<typename Client, typename Settings>
class AbstractOperation
{
public:
    AbstractOperation(const nx::utils::ArgumentParser& arguments):
        m_arguments(arguments)
    {
    }

    int run()
    {
        bool result = false;
        std::tie(m_settings, result) = Settings::read(m_arguments);
        if (!result)
            return 1;

        if constexpr (std::is_same_v<Settings, CommonSettings>)
            m_client = prepareApiClient(m_settings);
        else
            m_client = prepareApiClient(m_settings.common);

        return runImpl();
    }

protected:
    const nx::utils::ArgumentParser& m_arguments;
    Settings m_settings;
    std::unique_ptr<Client> m_client;

    virtual int runImpl() = 0;

    std::unique_ptr<Client> prepareApiClient(const CommonSettings& settings)
    {
        return std::make_unique<Client>(
            settings.region,
            nx::utils::Url(settings.url),
            nx::cloud::aws::Credentials(
                settings.user.c_str(),
                nx::network::http::PasswordAuthToken(settings.password.c_str())));
    }

    template<typename Func>
    struct DeriveResultTupleFromHandler;

    template<typename... Args>
    struct DeriveResultTupleFromHandler<nx::utils::MoveOnlyFunc<void(Args...)>>
    {
        using type = std::tuple<Args...>;
    };

    template <typename... Args>
    struct select_last;

    template <typename T>
    struct select_last<T>
    {
        using type = T;
    };

    template <typename T, typename... Args>
    struct select_last<T, Args...>
    {
        using type = typename select_last<Args...>::type;
    };

    template<typename... InputArgs, typename... FuncInputArgs,
        typename Handler = typename select_last<FuncInputArgs...>::type,
        typename OutputTuple = typename DeriveResultTupleFromHandler<Handler>::type
    >
    OutputTuple invoke(
        void (Client::*func)(FuncInputArgs...),
        InputArgs&&... inputArgs)
    {
        std::promise<OutputTuple> done;
        (m_client.get()->*func)(
            std::forward<InputArgs>(inputArgs)...,
            [&done](auto&&... result)
            {
                done.set_value(std::make_tuple(std::forward<decltype(result)>(result)...));
            });

        auto resultTuple = done.get_future().get();
        if (!std::get<0>(resultTuple).ok())
        {
            std::cerr << "Error: " << std::get<0>(resultTuple).text() << std::endl;
            return resultTuple;
        }

        return resultTuple;
    }
};

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

class UploadOperation:
    public AbstractOperation<nx::cloud::aws::s3::ApiClient, UploadSettings>
{
    using base_type = AbstractOperation;

public:
    using base_type::base_type;

    virtual int runImpl() override
    {
        std::ifstream contentFile(
            m_settings.inputFilePath,
            std::ios_base::in | std::ios_base::binary);
        if (!contentFile.is_open())
            return false;
        std::stringstream contents;
        contents << contentFile.rdbuf();

        std::string uploadPath;
        const nx::utils::Url url(m_settings.common.url.c_str());
        if (!nx::utils::filesystem::path(url.path().toStdString()).has_filename())
            uploadPath = nx::utils::filesystem::path(m_settings.inputFilePath).filename().string();

        const auto [uploadResult] = invoke(
            &nx::cloud::aws::s3::ApiClient::uploadFile,
            uploadPath,
            nx::Buffer::fromStdString(contents.str()));

        return uploadResult.ok() ? 0 : 2;
    }
};

//-------------------------------------------------------------------------------------------------

class DeleteOperation:
    public AbstractOperation<nx::cloud::aws::s3::ApiClient, CommonSettings>
{
    using base_type = AbstractOperation;

public:
    using base_type::base_type;

    virtual int runImpl() override
    {
        const auto [deleteResult] = invoke(&nx::cloud::aws::s3::ApiClient::deleteFile, "");
        return deleteResult.ok() ? 0 : 2;
    }
};

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

class DownloadOperation:
    public AbstractOperation<nx::cloud::aws::s3::ApiClient, DownloadSettings>
{
    using base_type = AbstractOperation;

public:
    using base_type::base_type;

    virtual int runImpl() override
    {
        const auto [downloadResult, fileContents] = invoke(
            &nx::cloud::aws::s3::ApiClient::downloadFile,
            "");
        if (!downloadResult.ok())
            return 2;

        return writeTo(m_settings.outputFilePath, fileContents);
    }

private:
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
};

//-------------------------------------------------------------------------------------------------

class StsAssumeRole:
    public AbstractOperation<nx::cloud::aws::sts::ApiClient, CommonSettings>
{
    using base_type = AbstractOperation;

public:
    using base_type::base_type;

    virtual int runImpl() override
    {
        nx::cloud::aws::sts::AssumeRoleRequest request;
        request.roleArn = "arn:aws:iam::009544449203:role/example-role";
        request.roleSessionName = "Test";

        const auto [result, assumeRoleResult] = invoke(
            &nx::cloud::aws::sts::ApiClient::assumeRole,
            request);
        if (!result.ok())
            return 2;

        std::cout << "Temporary credentials: " << std::endl <<
            assumeRoleResult.credentials.accessKeyId << ":" <<
            assumeRoleResult.credentials.secretAccessKey << std::endl <<
            "are valid until " << assumeRoleResult.credentials.expiration << std::endl;

        return 0;
    }
};

//-------------------------------------------------------------------------------------------------

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

    if (arguments.contains("sts-assume-role"))
        return StsAssumeRole(arguments).run();
    if (arguments.contains("u"))
        return UploadOperation(arguments).run();
    else if (arguments.contains("d"))
        return DeleteOperation(arguments).run();
    else
        return DownloadOperation(arguments).run();

    return 0;
}
