Library dependency when using ProxyDecoder:

mobile_client
    lib<ffmpeg-old>*.so # Without vdpau support.
    libnx_bpi_videonode_plugin.so # Gl Shader for native bpi YUV format.
    libnx_media.so
        libproxydecoder.so # Plugin into nx_media which supports bpi hw video engine.
            lib<ffmpeg-new>*.so # With vdpau support.       
            libvdpau.so # Default Bananian vdpau with dynamically-linked backends.
                libvdpau_sunxi.so # vdpau backend for bpi.
                    libcedrus.so # Kernel driver for bpi hw video engine.
                    libpixman-1.so # Software 2D drawing library.

---------------------------------------------------------------------------------------------------
- @bpi

# Test VDPAU:
killall lightdm
startx
# Use original ffmpeg from Bananian8:
export LD_LIBRARY_PATH=/usr/lib/arm-linux-gnueabihf/neon.BAK/vfp
mpv --vo=vdpau --hwdec=vdpau --hwdec-codecs=all /opt/media/...

# Mount Ubuntu via nfs:
mkdir -p ~/develop && mount -o nolock <ubuntu-ip>:/<ubuntu-path-to-develop> ~/develop

# Build and install libproxydecoder.so:
~/develop/third_party/bpi/libproxydecoder/build_lib.sh

# Run lite_client:
killall lightdm
cd /opt/networkoptix/lite_client
# Use latest ffmpeg with vdpau support:
export LD_LIBRARY_PATH=lib:/opt/ffmpeg/lib 
bin/mobile_client

---------------------------------------------------------------------------------------------------
- @ubunti

# Mount bpi via nfs to ubuntu:
mkdir -p /bpi; mount -o nolock <bpi-ip>:/ /bpi

cd ~/develop/netoptix_vms

# Initial build and install:
hg update bpi_videonode
mvn package -Darch=arm -Dbox=bpi -Dnewmobile=true
sudo cp -r build_environment/target-bpi/lib/debug/libnx_* /bpi/opt/networkoptix/lite_client/lib/
sudo cp -r build_environment/target-bpi/bin/debug/mobile_client /bpi/opt/networkoptix/lite_client/bin/

# Rebuild and install mobile_client:
make -C mobile_client/arm-bpi -f Makefile.debug -j12
sudo cp -r build_environment/target-bpi/bin/debug/mobile_client /bpi/opt/networkoptix/lite_client/bin/

# Rebuild and install nx_media:
make -C common_libs/nx_media/arm-bpi -f Makefile.debug -j12
sudo cp -r build_environment/target-bpi/lib/debug/libnx_media* /bpi/opt/networkoptix/lite_client/lib/
