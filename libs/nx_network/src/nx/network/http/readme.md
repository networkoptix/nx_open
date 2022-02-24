# HTTP framework {#nx_network_http}

// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

## REST API server example

Let's say we need to implement a "key: value" dictionary over HTTP.
This application have the following API:
- GET /elements/<key>. Returns element with the specified key or reports "404 Not Found"
- POST /elements. Adds a new element to the dictionary
- DELETE /elements/<key>. Removes an elements with a specified key.


    #include <map>

    #include <nx/network/socket_global.h>
    #include <nx/network/http/buffer_source.h>
    #include <nx/network/http/server/abstract_api_request_handler.h>
    #include <nx/network/http/server/http_server_builder.h>
    #include <nx/network/http/server/rest/http_server_rest_message_dispatcher.h>
    #include <nx/utils/argument_parser.h>
    #include <nx/utils/deprecated_settings.h>
    #include <nx/utils/system_error.h>
    #include <nx/utils/string.h>
    #include <nx/utils/scope_guard.h>

    // This is the application's business logic (key/value dictionary).
    // It is not related to HTTP at all.
    class Dictionary
    {
    public:
        std::optional<std::string /*value*/> get(const std::string& key) const;
        bool add(const std::string& key, const std::string& value);
        bool delete_(const std::string& key);

    private:
        std::map<std::string, std::string> m_data;
    };

    //-------------------------------------------------------------------------------------------------

    using namespace nx::network;

    // This class provides HTTP API to the business logic layer.
    class ApiServer
    {
    public:
        ApiServer(Dictionary* dictionary);

        void registerApi(http::server::rest::MessageDispatcher* dispatcher);

    private:
        Dictionary* m_dictionary = nullptr;
    };

    //-------------------------------------------------------------------------------------------------

    // Application usage example:
    // $ ./a.out --http/endpoints=127.0.0.1:12345
    // $ curl -X POST -H "Content-Type:text/plain" -d "Hello, world" http://127.0.0.1:12345/elements/key1
    //
    // $ curl http://127.0.0.1:12345/elements/key1
    // Hello, world

    int main(int argc, const char** argv)
    {
        using namespace nx::network;

        // Initializing network subsystem. This is usually created in the beginning of the application.
        SocketGlobalsHolder socketGlobals;

        //---------------------------------------------------------------------------------------------
        // Initializing the business logic layer.
        Dictionary dict;

        //---------------------------------------------------------------------------------------------
        // Initializing the HTTP API to our business logic.
        http::server::rest::MessageDispatcher dispatcher;
        ApiServer apiServer(&dict);
        apiServer.registerApi(&dispatcher);

        //---------------------------------------------------------------------------------------------
        // Launching HTTP server that invokes our API.

        // Reading HTTP server settings.
        http::server::Settings httpSettings;
        httpSettings.load(QnSettings(nx::utils::ArgumentParser(argc, argv)));

        // Building the server from settings.
        // Note: the builder will binds to endpoints, but does not start listening.
        auto [server, err] = http::server::Builder::build(
            httpSettings,
            nullptr, // AbstractAuthenticationManager can be used here to do authentication.
            &dispatcher);
        if (!server)
        {
            std::cerr << "Failed to start HTTP server. " <<
                SystemError::toString(err) << std::endl;
            return 1;
        }

        // Starting listening for incoming connections and requests.
        if (!server->listen())
        {
            std::cerr<<"Failed to listen "<<nx::toString(server->endpoints()).toStdString()<<
                ". "<<SystemError::getLastOSErrorText()<<std::endl;
            return 2;
        }

        for (std::string s; s != "stop"; std::cin >> s) {}

        return 0;
    }

    //-------------------------------------------------------------------------------------------------

    std::optional<std::string /*value*/> Dictionary::get(const std::string& key) const
    {
        auto it = m_data.find(key);
        return it != m_data.end() ? std::make_optional(it->second) : std::nullopt;
    }

    bool Dictionary::add(const std::string& key, const std::string& value)
    {
        return m_data.emplace(key, value).second;
    }

    bool Dictionary::delete_(const std::string& key)
    {
        return m_data.erase(key) > 0;
    }

    //-------------------------------------------------------------------------------------------------

    ApiServer::ApiServer(Dictionary* dictionary): m_dictionary(dictionary) {}

    void ApiServer::registerApi(http::server::rest::MessageDispatcher* dispatcher)
    {
        // Add element to the dictionary.
        dispatcher->registerRequestProcessorFunc(
            http::Method::post,
            "/elements/{key}",
            [this](
                http::RequestContext requestCtx,
                http::RequestProcessedHandler completionHandler)
            {
                m_dictionary->add(
                    requestCtx.requestPathParams.getByName("key"),
                    requestCtx.request.messageBody.takeStdString());
                completionHandler(http::StatusCode::ok);
            });

        // Get element.
        dispatcher->registerRequestProcessorFunc(
            http::Method::get,
            "/elements/{key}",
            [this](
                http::RequestContext requestCtx,
                http::RequestProcessedHandler completionHandler)
            {
                auto value = m_dictionary->get(requestCtx.requestPathParams.getByName("key"));
                if (!value)
                    return completionHandler(http::StatusCode::notFound);

                http::RequestResult result(http::StatusCode::ok);
                result.dataSource = std::make_unique<http::BufferSource>(
                    "text/plain", std::move(*value));
                completionHandler(std::move(result));
            });

        // Delete element.
        dispatcher->registerRequestProcessorFunc(
            http::Method::delete_,
            "/elements/{key}",
            [this](
                http::RequestContext requestCtx,
                http::RequestProcessedHandler completionHandler)
            {
                completionHandler(
                    m_dictionary->delete_(requestCtx.requestPathParams.getByName("key"))
                    ? http::StatusCode::ok
                    : http::StatusCode::notFound);
            });
    }

## Streaming data over HTTP

Streaming data in request/response is done via nx::network::http::AbstractMsgBodySource class.
It provides basic asynchronous data source API.
In case of a request it reads from HTTP connection.
In case of a response, the HTTP handler provides this class pointer to the HTTP subsystem.

See `nx::network::http::MessageBodyDeliveryType`, `nx::network::http::AbstractMsgBodySource`.

TODO
