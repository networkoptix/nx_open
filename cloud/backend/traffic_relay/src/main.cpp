#include <nx/cloud/utils/service/run.h>
#include <nx/cloud/relay/libtraffic_relay_main.h>

int main(int argc, char* argv[])
{
    return nx::cloud::utils::service::run(
        argc,
        argv,
        &nx::cloud::relay::trafficRelayMain);
}
