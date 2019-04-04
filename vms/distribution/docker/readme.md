# networkoptix-mediaserver #

This is a docker image based on `ubuntu` that runs `networkoptix-mediaserver` with a minimal set of services.

## Building ##

TODO: Pass the name for mediaserver.deb through arguments

Building an image from current directory
```bash
docker build -t mediaserver --build-arg mediaserver_deb=path_to_mediaserver.deb .
```

It will fetch all necessary layers and build docker image with name `mediaserver`.

## Running ##

systemd needs `/run`, `/run/lock` to be present, so we need to map them
Also we need to have access to /sys/fs/cgroup:/sys/fs/cgroup

Running it
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

1. Interaction with cmake
2. Provide a better ways to specify deb file for mediaserver
3. Provide some scripts to simplify start&restart
