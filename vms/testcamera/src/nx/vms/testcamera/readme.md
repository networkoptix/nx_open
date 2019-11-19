# testcamera command-line tool

---------------------------------------------------------------------------------------------------
## Introduction

This command-line tool creates a virtual (software) network camera which can be discovered by a
VMS Server and streams the specified video files.

Run with `-h` or `--help` to see usage instructions.

Advanced parameters can be tuned via ini-config `test_camera.ini`.

---------------------------------------------------------------------------------------------------
## Architecture

Here is the tree of entities where each entity uses its sub-entities.

- `main()`: Initializes mechanisms, parses command-line arguments, loads video files and starts
    the camera discovery service.
    - `CliOptions`: Parses and validates command-line arguments and stores them.
    - `CameraOptions`: Value object with the values of options of a particular camera, extracted
        from `CliOptions`.
    - `FileCache`: Loads video files into memory before camera threads are started.
    - `CameraPool`: Is populated with cameras, and then starts the camera discovery service.
        - `FrameLogger`: If requested via .ini, logs each sent frame into a file. Only one instance
            of `FrameLogger` exists, owned by `CameraPool` and supplied to its `Camera` instances.
        - `CameraDiscoveryListener`: Runs the listener thread which receives discovery messages and
            sends responses.
        - `CameraRequestProcessor`: Runs the thread which handles the streaming requests from
            Servers: receives a URL from a Server, finds the respective `Camera` in the
            `CameraPool`, and asks it to perform streaming for the Server.
        - `Camera`: Performs streaming of the specified stream (primary or secondary) of the given
            file set (taking the file data from the `FileCache`) to the given socket. Streams in an
            infinite loop, exiting only on error.
            - `FileStreamer`: Performs streaming of a single given video file - sends each frame to
                the given socket. Performs PTS unlooping/shifting when needed; for this purpose,
                receives an instance of `PtsUnloopingContext` which persists for all streaming
                iterations of the particular file.
