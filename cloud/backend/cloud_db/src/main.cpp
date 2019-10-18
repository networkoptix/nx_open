#include <nx/cloud/db/lib_cloud_db_main.h>
#include <nx/cloud/utils/service/run.h>

int main(int argc, char* argv[])
{
    return nx::cloud::utils::service::run(
        argc, 
        argv,
        &libCloudDBMain);
}
