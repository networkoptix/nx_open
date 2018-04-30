
cd ~/develop/nx_vms/cloud_portal/build_scripts
./build.sh
cd ~/develop/nx_vms/nx_cloud_deploy/cloud_portal
./make.sh stage pack
cd ~/gen/dev
docker-compose up -d
