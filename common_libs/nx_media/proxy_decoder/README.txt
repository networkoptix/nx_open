Library dependency when using ProxyDecoder:

mobile_client
    lib<ffmpeg-old>*.so
    libnx_media.so
        libproxydecoder.so
            lib<ffmpeg-new>*.so        
            libvdpau.so
                libvdpau_sunxi.so
                libcedrus.so
                libpixman-1.so

---------------------------------------------------------------------------------------------------
- @nx1

# Test VDPAU:
killall lightdm
startx
# Use original ffmpeg from Bananian8:
export LD_LIBRARY_PATH=/usr/lib/arm-linux-gnueabihf/neon.BAK/vfp
mpv --vo=vdpau --hwdec=vdpau --hwdec-codecs=all /opt/media/...

# Mount Ubuntu via nfs:
mkdir -p ~/develop && mount -o nolock <ubuntu-ip>:/<ubuntu-path-to-develop> ~/develop

# Build and install libproxydecoder.so:
~/develop/netoptix_vms/common_libs/nx_media/proxy_decoder/build_lib.sh

# Run lite_client:
killall lightdm
startx
cd /opt/networkoptix/lite_client
# Use latest ffmpeg with vdpau support:
export LD_LIBRARY_PATH=lib:/opt/ffmpeg/lib 
bin/mobile_client

---------------------------------------------------------------------------------------------------
- @ubunti

# Mount nx1 via nfs to ubuntu:
mkdir -p /nx1; mount -o nolock <nx1-ip>:/ /nx1

cd ~/develop/netoptix_vms

# Initial build and install:
hg update proxy_decoder 
mvn package -Darch=arm -Dbox=bpi -Dnewmobile=true
sudo cp -r build_environment/target-bpi/lib/debug/libnx_* /nx1/opt/networkoptix/lite_client/lib/
sudo cp -r build_environment/target-bpi/bin/debug/mobile_client /nx1/opt/networkoptix/lite_client/bin/

# Rebuild and install mobile_client:
make -C mobile_client/arm-bpi -f Makefile.debug -j12
sudo cp -r build_environment/target-bpi/bin/debug/mobile_client /nx1/opt/networkoptix/lite_client/bin/

# Rebuild and install nx_media:
make -C common_libs/nx_media/arm-bpi -f Makefile.debug -j12
sudo cp -r build_environment/target-bpi/lib/debug/libnx_media* /nx1/opt/networkoptix/lite_client/lib/
