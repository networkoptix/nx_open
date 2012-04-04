import os

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
