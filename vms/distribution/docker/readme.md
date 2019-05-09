# networkoptix-mediaserver #

This is a docker image based on `ubuntu` that runs `networkoptix-mediaserver` with a minimal set of services.

## Building ##

Building an image from current directory

You can use build.sh utility:

```bash
# Building from existing debian package. It will copy it to build folder and build an image.
build.sh ~/Downloads/nxwitness-server-4.0.0.28737-linux64-beta-test.deb

# Or using url to debian package. It will download it and build an image.
build.sh https://beta.networkoptix.com/beta-builds/default/28608/linux/nxwitness-server-4.0.0.28608-linux64-beta-prod.deb
```

The script will use current directory as a docker workspace. It also copies necessary files (deb package) to 
this folder. This allows to run `build.sh` out of `nx_vms` source folder.

You can use docker directly:

```bash
docker build -t mediaserver --build-arg mediaserver_deb=path_to_mediaserver.deb .
```

It will fetch all necessary layers and build docker image with name `mediaserver`.

## Altering cloud_host ##

build.sh can override cloud host setting for mediaserver:

```bash
build.sh --cloud cloud-dev2.hdw.mx ~/Downloads/nxwitness-server-4.0.0.28737-linux64-beta-test.deb
```

It sets `cloud_host` build argument for docker. You can invoke it directly:

```bash
docker build -t mediaserver --build-arg cloud_host=cloud-dev2.hdw.mx --build-arg mediaserver_deb=path_to_mediaserver.deb .
```

It is possible to change cloud host for existing container instance as well: 

```bash
sudo docker exec -i -t mediaserver1 /setup/manage.sh --cloud cloud-dev2.hdw.mx
```

## Running ##

systemd needs `/run`, `/run/lock` to be present, so we need to map them
Also container needs `/sys/fs/cgroup:/sys/fs/cgroup` to be mapped

Running it:

```bash
sudo docker run -d --name mediaserver1 --tmpfs /run --tmpfs /run/lock -v /sys/fs/cgroup:/sys/fs/cgroup:ro -t mediaserver
```
OR
You can use the docker-compose file which has the run parameters set, and will provide volumes for persistent database and video storage.
Note that the video storage location, if modified, needs to be short. Changing the name is fine but changing the path may result in no valid storage location.

## Useful commands ##

Entering bash inside named container:
```bash
sudo docker exec -i -t mediaserver1 /bin/bash #by Name
```

Reading system logs:

```bash
sudo docker logs mediaserver1
```

Reading journalctl:

```bash
sudo docker exec -i -t mediaserver1 journalctl -u networkoptix-mediaserver
```

Stopping container and resetting all the data:

```
sudo docker stop mediaserver1 && sudo docker rm mediaserver1
```

## TODO ##

1. ~~Provide a better ways to specify deb file for mediaserver.~~ Done
1. Provide some scripts to simplify start&restart.
1. Interaction with cmake.
