import qbs

Project
{
    references: [
        "cloud_db_client",
        "libvms_gateway",
        "libconnection_mediator",
        "libcloud_db",
        "vms_gateway",
        "connection_mediator",
        "cloud_db",
        "cloud_connect_auto_test",
        "cloud_connect_test_util",
        "time_server"
    ]
}
