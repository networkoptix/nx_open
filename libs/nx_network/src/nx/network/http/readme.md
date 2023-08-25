# HTTP framework {#nx_network_http}

// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

## Overview

The key elements of the HTTP server is the listener and request handler.
A listener is created by nx::network::http::server::Builder::build function.
Request handler inherits nx::network::http::AbstractRequestHandler class. A request listener can do
any kind of job such as:
- authentication
- dispatching requests based on various rules
- implementing application's business logic

For example, a typical HTTP authenticator (nx::network::http::server::AuthenticationManager) is a
request handler that implements HTTP authentication and forwards the request down the processing
pipeline on authentication success or replies with 401 HTTP status on authentication failure.

Request handler chaining is used to create HTTP processing pipelines. Common pipeline is
authenticator -> dispatcher -> business logic handler.

## REST API server example

The example below demonstrates an HTTP server implementing "key: value" dictionary over HTTP.
This application has the following API:
- GET /elements/<key>. Returns element with the specified key or reports "404 Not Found".
- PUT /elements/<key>. Adds new or replaces existing element in the dictionary.
- DELETE /elements/<key>. Removes an element with a specified key.

All requests require authentication with predefined credentials "admin:admin".


    #include <initializer_list>
    #include <unordered_map>

    #include <nx/network/socket_global.h>
    #include <nx/network/http/buffer_source.h>
    #include <nx/network/http/server/abstract_api_request_handler.h>
    #include <nx/network/http/server/http_server_builder.h>
    #include <nx/network/http/server/http_server_authentication_manager.h>
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
        std::optional<std::string /*value*/> get(const std::string& key) const
        {
            auto it = m_data.find(key);
            return it != m_data.end() ? std::make_optional(it->second) : std::nullopt;
        }

        // Adds new elements or replaces existing element with the given key.
        void save(const std::string& key, const std::string& value)
        {
            m_data[key] = value;
        }

        // Erase element with the given key.
        // Returns true if element was found and removed, false if not found.
        bool delete_(const std::string& key)
        {
            return m_data.erase(key) > 0;
        }

    private:
        std::unordered_map<std::string, std::string> m_data;
    };

    //-------------------------------------------------------------------------------------------------

    using namespace nx::network;

    // Provides HTTP API to the business logic layer. This class is layer that converts HTTP calls
    // into Dictionary class invocations.
    class ApiServer
    {
    public:
        ApiServer(Dictionary* dictionary);

        // Registers neccessary HTTP handlers under the dispatcher.
        void registerApi(http::server::rest::MessageDispatcher* dispatcher);

    private:
        Dictionary* m_dictionary = nullptr;
    };

    //-------------------------------------------------------------------------------------------------
    // Simple class that represents a credentials source.

    class CredentialsProvider: public http::server::AbstractAuthenticationDataProvider
    {
    public:
        CredentialsProvider(std::initializer_list<std::pair<const std::string, std::string>> credentials):
            m_credentials(std::move(credentials))
        {
        }

        virtual void getPasswordByUserName(
            const std::string& userName,
            AbstractAuthenticationDataProvider::LookupResultHandler completionHandler) override
        {
            if (m_credentials.contains(userName))
            {
                completionHandler({
                    http::server::PasswordLookupResult::Code::ok,
                    http::PasswordAuthToken(m_credentials.at(userName))});
            }
            else
            {
                completionHandler({http::server::PasswordLookupResult::Code::notFound, {}});
            }
        }

    private:
        std::unordered_map</*userName*/ std::string, /*password*/ std::string> m_credentials;
    };

    //-------------------------------------------------------------------------------------------------

    // Application usage example:
    // $ ./a.out --http/endpoints=127.0.0.1:12345
    // $ curl --digest --user admin:admin -X PUT -H "Content-Type:text/plain" -d "Hello, world" http://127.0.0.1:12345/elements/key1
    // Hello, world
    // $ curl --digest --user admin:admin http://127.0.0.1:12345/elements/key1
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
        // The dispatcher is reponsible for forwarding request to the appropriate application handler.
        http::server::rest::MessageDispatcher dispatcher;

        //---------------------------------------------------------------------------------------------
        // Initializing the HTTP API to our business logic.
        ApiServer apiServer(&dict);
        apiServer.registerApi(&dispatcher);

        //---------------------------------------------------------------------------------------------
        // Initializing authentication.
        CredentialsProvider credsProvider({{"admin", "admin"}});

        http::server::AuthenticationManager authenticator(
            &dispatcher,    //< If request authenticated successfully, pass request to the dispatcher.
            &credsProvider);
        // NOTE: The authenticator will receive every HTTP request. When it is desired to use
        // different authenticators for different request paths,
        // nx::network::http::server::AuthenticationDispatcher should be used.

        //---------------------------------------------------------------------------------------------
        // Launching HTTP server that invokes our API.

        // Reading HTTP server settings.
        http::server::Settings httpSettings;
        httpSettings.load(QnSettings(nx::utils::ArgumentParser(argc, argv)));

        // Building the HTTP listener from settings.
        // Note: the builder will bind listener to endpoints, but won't start listening.
        // The HTTP listener will submit every request to the authenticator.
        auto [server, err] = http::server::Builder::build(httpSettings, &authenticator);
        if (!server)
        {
            std::cerr << "Failed to start HTTP server. " << SystemError::toString(err) << std::endl;
            return 1;
        }

        // Starting listening for incoming connections and requests.
        if (!server->listen())
        {
            std::cerr<<"Failed to listen "<<nx::toString(server->endpoints()).toStdString()<<
                ". "<<SystemError::getLastOSErrorText()<<std::endl;
            return 2;
        }

        // Waiting for the user input to stop the application.
        for (std::string s; s != "stop"; std::cin >> s) {}

        return 0;
    }

    //-------------------------------------------------------------------------------------------------
    // ApiServer class implementation.

    ApiServer::ApiServer(Dictionary* dictionary): m_dictionary(dictionary) {}

    void ApiServer::registerApi(http::server::rest::MessageDispatcher* dispatcher)
    {
        // Add element to the dictionary.
        dispatcher->registerRequestProcessorFunc(
            http::Method::put,
            "/elements/{key}",
            [this](
                http::RequestContext requestCtx,
                http::RequestProcessedHandler completionHandler)
            {
                std::string value = requestCtx.request.messageBody.takeStdString();
                m_dictionary->save(requestCtx.requestPathParams.getByName("key"), value);

                http::RequestResult result(http::StatusCode::ok);
                result.body = std::make_unique<http::BufferSource>("text/plain", std::move(value));
                completionHandler(std::move(result));
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
                result.body = std::make_unique<http::BufferSource>("text/plain", std::move(*value));
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
