#include <QCoreApplication>

#include <nx/network/test_support/run_test.h>
#include <nx/utils/argument_parser.h>
#include <nx/utils/test_support/run_test.h>
#include <nx/utils/move_only_func.h>

#include <common/static_common_module.h>

int main(int argc, char** argv)
{
    QnStaticCommonModule staticCommonModule(Qn::PeerType::PT_Server);

    std::unique_ptr<QCoreApplication> application;
    return nx::network::test::runTest(argc, argv,
        [&](const nx::utils::ArgumentParser& /*args*/)
        {
            application.reset(new QCoreApplication(argc, argv));
            return nx::utils::test::DeinitFunctions();
        });
}
