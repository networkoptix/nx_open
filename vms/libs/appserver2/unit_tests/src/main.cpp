#include <QCoreApplication>

#include <nx/network/test_support/run_test.h>
#include <nx/utils/argument_parser.h>
#include <nx/utils/test_support/run_test.h>
#include <nx/utils/move_only_func.h>

#include <common/static_common_module.h>
#include <common/common_meta_types.h>

int main(int argc, char** argv)
{
    QnStaticCommonModule staticCommonModule(nx::vms::api::PeerType::server);

    std::unique_ptr<QCoreApplication> application;
    QnCommonMetaTypes::initialize();
    return nx::network::test::runTest(argc, argv,
        [&](const nx::utils::ArgumentParser& /*args*/)
        {
            application.reset(new QCoreApplication(argc, argv));
            return nx::utils::test::DeinitFunctions();
        });
}
