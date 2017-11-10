#include <media_server_process.h>
#include <gtest/gtest.h>

namespace nx {
namespace mediaserver_core {
namespace test {

TEST(MediaServerProcess, ApiRestrictions)
{
    using namespace nx_http;
    static const std::map<const char*, unsigned int> kExpectedAuthMethods =
    {
        {"/ec2/getCurrentTime", AuthMethod::NotDefined},
        {"/api/getCurrentTime", AuthMethod::NotDefined},

        {"*", AuthMethod::noAuth},

        {"/api/ping", AuthMethod::noAuth},
        {"/web/api/ping", AuthMethod::noAuth},
        {"/proxy/{28c0b696-bfc6-11e7-bd5c-9775e072ea29}/api/ping", AuthMethod::noAuth},
        {"/web/proxy/{server}/api/ping", AuthMethod::noAuth},
        {"/ec2/getCurrentTime/api/ping", AuthMethod::NotDefined},

        {"/api/camera_event", AuthMethod::noAuth},
        {"/api/camera_event_what/ever", AuthMethod::noAuth},

        {"/api/showLog", AuthMethod::urlQueryParam},
        {"/api/showLogAnd/more", AuthMethod::urlQueryParam},

        {"/api/moduleInformation", AuthMethod::noAuth},
        {"/web/api/moduleInformation", AuthMethod::noAuth},

        {"/static/index.html", AuthMethod::noAuth},
        {"/static/script.js", AuthMethod::noAuth},
        {"/static/script.js", AuthMethod::noAuth},

        {"/api/installUpdateUnauthenticated", AuthMethod::noAuth},
        {"/web/api/installUpdateUnauthenticated", AuthMethod::noAuth},
        {"/web/proxy/127.0.0.1:7001/api/installUpdateUnauthenticated", AuthMethod::noAuth},
    };

    AuthMethodRestrictionList restrictions;
    MediaServerProcess::configureApiRestrictions(&restrictions);
    for (const auto& method: kExpectedAuthMethods)
    {
        Request request;
        request.requestLine.url.setPath(QString::fromLatin1(method.first));
        EXPECT_EQ(
            AuthMethodRestrictionList::kDefaults | method.second,
            restrictions.getAllowedAuthMethods(request)) << method.first;
    }
}

} // namespace test
} // namespace mediaserver_core
} // namespace nx
