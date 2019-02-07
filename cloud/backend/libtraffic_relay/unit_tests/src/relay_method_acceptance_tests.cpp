#include "relay_method_acceptance_tests.h"

#include <boost/preprocessor/seq/for_each.hpp>

#include <nx/network/cloud/tunnel/relay/api/relay_api_client_over_http_upgrade.h>
#include <nx/network/cloud/tunnel/relay/api/relay_api_client_over_http_get_post_tunnel.h>
#include <nx/network/cloud/tunnel/relay/api/relay_api_client_over_http_connect.h>

namespace nx::cloud::relay::test {

#define DETAIL_INSTANTIATE_RELAY_METHOD_TESTS(ClientSideClass, ServerSideClass, StructName, CaseName) \
    struct StructName \
    { \
        using ClientSideApiClient = api::detail::ClientSideClass; \
        using ServerSideApiClient = api::detail::ServerSideClass; \
    }; \
    \
    INSTANTIATE_TYPED_TEST_CASE_P( \
        CaseName, \
        RelayMethodAcceptance, \
        StructName);

#define INSTANTIATE_RELAY_METHOD_TESTS(r, ClientSideClass, ServerSideClass) \
    DETAIL_INSTANTIATE_RELAY_METHOD_TESTS( \
        ClientSideClass, \
        ServerSideClass, \
        BOOST_PP_CAT(ClientSideClass, ServerSideClass), \
        BOOST_PP_CAT(BOOST_PP_CAT(ClientSideClass, __vs__), ServerSideClass))

//-------------------------------------------------------------------------------------------------

#define RelayClientTypes \
    (ClientOverHttpUpgrade)\
    (ClientOverHttpGetPostTunnel)\
    (ClientOverHttpConnect)

BOOST_PP_SEQ_FOR_EACH(
    INSTANTIATE_RELAY_METHOD_TESTS,
    ClientOverHttpUpgrade,
    RelayClientTypes)

BOOST_PP_SEQ_FOR_EACH(
    INSTANTIATE_RELAY_METHOD_TESTS,
    ClientOverHttpGetPostTunnel,
    RelayClientTypes)

BOOST_PP_SEQ_FOR_EACH(
    INSTANTIATE_RELAY_METHOD_TESTS,
    ClientOverHttpConnect,
    RelayClientTypes)

} // namespace nx::cloud::relay::test
