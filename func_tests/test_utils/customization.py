"""Customization support module

Load information related to [current] customization.
"""

import debian.debfile


# customization company name is the directory name under '/opt/': '/opt/networkoptix' -> company name = 'networkoptix'
def detect_customization_company_name(mediaserver_dist_fpath):
    debfile = debian.debfile.DebFile(mediaserver_dist_fpath)
    for tar_info in debfile.data.tgz().getmembers():
        path = tar_info.path.split('/')
        if len(path) < 3 or path[:2] != ['.', 'opt']:
            continue
        return path[2]
    assert False, 'Unable to detect company name from mediaserver distributive %r contents' % mediaserver_dist_fpath
