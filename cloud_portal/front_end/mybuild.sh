
cd ~/develop/nx_vms/cloud_portal/build_scripts
./build.sh
cd ~/develop/nx_vms/cloud/deploy/cloud_portal
./make.sh stage pack
cd ~/gen/dev
docker-compose up -d
