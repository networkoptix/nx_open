nx_add_test(nx_aws_client_ut
    WERROR_IF NOT WINDOWS
    PUBLIC_LIBS nx_aws_client)
