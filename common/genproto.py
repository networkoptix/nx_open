# /usr/bin/xsd cxx-tree --generate-serialization --output-dir build/debug/generated --cxx-regex "/^(.*).xsd/xsd_\1.cpp/" --hxx-regex "/^(.*).xsd/xsd_\1.h/" --generate-ostream --root-element recordedTimePeriods src/api/xsd/videoserver/recordedTimePeriods.xsd

import os

def generate_xsd(toolsdir, outdir): # build/debug/generated
    XSD_FILES = ('src/api/xsd/cameras.xsd',
                 'src/api/xsd/layouts.xsd',
                 'src/api/xsd/users.xsd',
                 'src/api/xsd/resourceTypes.xsd',
                 'src/api/xsd/resources.xsd',
                 'src/api/xsd/resourcesEx.xsd',
                 'src/api/xsd/servers.xsd',
                 'src/api/xsd/storages.xsd',
                 'src/api/xsd/scheduleTasks.xsd',
                 'src/api/xsd/events.xsd',
                 'src/api/xsd/videoserver/recordedTimePeriods.xsd')

    output_files = []
    for xfile in XSD_FILES:
        basename = os.path.basename(xfile) # cameras.xsd
        filebase = os.path.splitext(basename)[0] # cameras
        output_files.append('%s/xsd_%s.cpp' % (outdir, filebase))
        os.system('%s/bin/xsd cxx-tree --generate-serialization --output-dir %s --cxx-regex \"/^(.*).xsd/xsd_\\1.cpp/\" --hxx-regex \"/^(.*).xsd/xsd_\\1.h/\" --generate-ostream --root-element %s %s' % (toolsdir, outdir, filebase, xfile))

    return output_files

# /usr/bin/protoc --proto_path=src/api/pb --cpp_out=build/debug/generated src/api/pb/camera.proto
def generate_pb(toolsdir, outdir):
    PB_FILES = ('src/api/pb/camera.proto',
                'src/api/pb/user.proto',
                'src/api/pb/layout.proto',
                'src/api/pb/resourceType.proto',
                'src/api/pb/resource.proto',
                'src/api/pb/server.proto',
                'src/api/pb/license.proto',
                'src/api/pb/ms_recordedTimePeriod.proto')

    output_files = []
    for xfile in PB_FILES:
        dirname = os.path.dirname(xfile) # src/api/pb
        basename = os.path.basename(xfile) # camera.proto
        filebase = os.path.splitext(basename)[0] # camera
        output_files.append('%s/%s.pb.cc' % (outdir, filebase))
        os.system('%s/bin/protoc --proto_path=%s --cpp_out=%s %s' % (toolsdir, dirname, outdir, xfile))

    return output_files
