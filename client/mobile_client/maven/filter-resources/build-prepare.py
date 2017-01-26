#!/usr/bin/env python
    if os.path.isfile("${provisioning_profile}"):
        shutil.copyfile("${provisioning_profile}", dst_file)