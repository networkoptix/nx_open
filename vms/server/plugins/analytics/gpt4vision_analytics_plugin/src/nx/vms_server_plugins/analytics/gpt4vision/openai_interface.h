// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

/**@file
 * This header defines structures used for OpenAI interoperation in the json format.
 */

#include <optional>
#include <string>
#include <vector>

#include <nx/reflect/instrument.h>

namespace nx::vms_server_plugins::analytics::gpt4vision {
namespace open_ai {

struct QueryPayload
{
    struct Message
    {
        struct Content
        {
            struct ImageUrl
            {
                std::string url;
            };

            std::string type;
            std::optional<std::string> text;
            std::optional<ImageUrl> image_url;
        };

        std::string role = "user";
        std::vector<Content> content;
    };

    std::string model = "gpt-4-vision-preview";
    int max_tokens = 4096;
    std::vector<Message> messages;
};
NX_REFLECTION_INSTRUMENT(QueryPayload::Message::Content::ImageUrl, (url))
NX_REFLECTION_INSTRUMENT(QueryPayload::Message::Content, (type)(text)(image_url))
NX_REFLECTION_INSTRUMENT(QueryPayload::Message, (role)(content))
NX_REFLECTION_INSTRUMENT(QueryPayload, (model)(max_tokens)(messages))

struct Response
{
    struct Choice
    {
        struct Message
        {
            std::string content;
        };

        Message message;
    };

    struct Error
    {
        std::string message;
        std::string type;
        std::string param;
        std::string code;
    };

    std::vector<Choice> choices;
    Error error;
};
NX_REFLECTION_INSTRUMENT(Response::Choice::Message, (content))
NX_REFLECTION_INSTRUMENT(Response::Choice, (message))
NX_REFLECTION_INSTRUMENT(Response::Error, (message)(type)(param)(code))
NX_REFLECTION_INSTRUMENT(Response, (choices)(error))

} // namespace open_ai
} // namespace nx::vms_server_plugins::analytics::gpt4vision
