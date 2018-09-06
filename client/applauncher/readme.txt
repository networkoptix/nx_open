Launches specified in settings client version.

Accepts tasks from clients and other launcher instances via named pipe.
Task has following format:

    {command name}\n
    {command parameter 1}\n
    {command parameter 2}\n
    ...
    {command parameter N}\n
    \n

For example, run client command look like:

    run\n
    1.4.3\n
    -e --log-level=DEBUG\n
    \n

Response has following format:

    {result code}\n
    {response parameter 1}\n
    {response parameter 2}\n
    ...
    {response parameter N}\n
    \n

Response example:

    ok\n
    \n

First line (required) - is a command. At the moment only "run" is supported.
Second line (required) - is a version to run
Third line (optinal) - command arguments
