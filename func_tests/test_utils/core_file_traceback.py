import os
import os.path
from .template_renderer import TemplateRenderer


GDB_TIMEOUT_SEC = 10*60  # Error in gdb commands cause gdb to not quit
WORK_DIR = '/tmp/gdb-extract-tb'


# all paths are remote, on specified host
# gdb executed on the same host where core file and binaries are stored
def create_core_file_traceback(host, bin_path, lib_dir, core_path):
    gdb_commands = TemplateRenderer().render(
        'gdb_extract_traceback.jinja2',
        BIN_PATH=bin_path,
        LIB_DIR=lib_dir,
        CORE_PATH=core_path,
        )
    host.mk_dir(WORK_DIR)
    commands_path = os.path.join(WORK_DIR, 'gdb_commands.%s' % os.urandom(3).encode('hex'))
    host.write_file(commands_path, gdb_commands)
    traceback = host.run_command(['gdb',  '--quiet', '-x', commands_path], cwd=WORK_DIR, timeout=GDB_TIMEOUT_SEC)
    return traceback
