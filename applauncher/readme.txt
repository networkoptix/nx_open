Launches specified in settings client version.
If no such client version installed, downloads it, installs and launches...


Accepts tasks from clients and other launcher instances via named pipe.
Task has following format:

    run\n
    1.4.3\n
    -e --log-level=DEBUG\n
    \n
    
First line (required) - is a command. At the moment only "run" is supported.
Second line (required) - is a version to run
Third line (optinal) - command arguments


Installation files are downloaded one-by-one
