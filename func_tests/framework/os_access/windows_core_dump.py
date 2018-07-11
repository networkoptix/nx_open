

def parse_dump(os_access, dump_path, symbols_path):
    return os_access.run_command(
        [
            # Its folder is not added to %PATH% by Chocolatey, but installation location is known (default).
            r'C:\Program Files (x86)\Windows Kits\10\Debuggers\x64\cdb.exe',
            '-z', dump_path,
            '-y', symbols_path,
            '-c',  # Execute commands:
            '.exr -1;'  # last exception,
            '.ecxr;'  # last exception context,
            'kc;'  # current thread backtrace,
            '~*kc;'  # all threads backtrace,
            'q',  # exit.
            '-n',  # Verbose symbols loading.
            '-s',  # Disable deferred symbols load to make stack clear of loading errors and warnings.
            ],
        timeout_sec=600,
        )
