import os
import shutil
import glob

def copy_file(src, dst_dir):
    dst_file = os.path.join(dst_dir, os.path.basename(src))
    if os.path.isfile(dst_file):
        os.remove(dst_file)

    if os.path.islink(src):
        link = os.readlink(src)
        os.symlink(link, dst_file)
    else:
        shutil.copy2(src, dst_file)

#Supports templates such as bin/*.dll
def copy_recursive(src, dst):
    if "*" in src:
        entries = glob.glob(src)
    else:
        entries = [ src ]

    for entry in entries:
        if os.path.isfile(entry):
            copy_file(entry, dst)

        elif os.path.isdir(entry):
            dst_basedir = os.path.join(dst, os.path.basename(entry))

            for dirname, _, filenames in os.walk(entry):
                rel_dir = os.path.relpath(dirname, entry)
                dst_dir = os.path.abspath(os.path.join(dst_basedir, rel_dir))

                if not os.path.isdir(dst_dir):
                    os.makedirs(dst_dir)

                for filename in filenames:
                    copy_file(os.path.join(dirname, filename), dst_dir)
