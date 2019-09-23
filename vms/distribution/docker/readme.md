# Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/ #

## Support (in plain terms, can probably be made more friendly) ##
XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX

EEEEE   X   X    PPPPP    EEEEE    RRRRR    IIIII    M       M   EEEEE    N   N   TTTTT    AA      L
E        X X     P    P   E        R    R     I      MM     MM   E        NN  N     T     A  A     L
EEEEE     X      PPPPP    EEEEE    RRRRR      I      M M   M M   EEEEE    N N N     T    AAAAAA    L
E        X X     P        E        R  R       I      M  M M  M   E        N  NN     T    A    A    L
EEEEE   X   X    P        EEEEE    R   R    IIIII    M   M   M   EEEEE    N   N     T    A    A    LLLLL

XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
  
    * This is an experimental feature.  
    * This has not been tested to be functional in unusual or edge cases.
    * It is not recommended for critical systems.
    * We can provide some support but knowledge of Docker is expected when using this feature.
    * In short, we won’t provide support on using Docker itself.  If you don’t already know how to run a container we at the most will send you to the Docker documentation.  But we will put forth reasonable efforts to resolve VMS-related issues that you may stumble upon in a patch or in future releases. No promises, though.  Please, test carefully if you intend to use this on a production system.

# networkoptix-mediaserver #

This is a docker image based on `ubuntu` that runs `networkoptix-mediaserver` with a minimal set of services.

## Building ##

Building an image from current directory

You can use build.sh utility:

```bash
# Building from existing debian package. It will copy it to the build folder and build the image.
build.sh ~/Downloads/nxwitness-server-4.0.0.28737-linux64-beta-test.deb

# Or using url. It will download the debian package and build the image.
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

## Networking ##

Networking
The network setting of a dockerized media server is very important.  Here are the network types you can use, and the results you can expect.

Host (recommended)
Licenses in this mode will always have the MAC of the host and the MAC can't be modified in "host" mode.

Pros:
    * You don't have to do any network configuration
    * Auto-discovery works
    * Other systems on the network can see your server
Cons:
    * You won't be able to run multiple servers on the same host without conflict as they use the IP of the host
    * Will not work on MacOS
    
Bridge
Pros:
    * You don't have to do any network configuration
    * Works on MacOS
Cons:
    * Licenses are reliant on the container's MAC address.  The MAC address can change if you start up servers in a different order invalidating the license as well as moving an image to another system.  This can be solved is you specify MAC address in the docker run or docker-compose.
    * Must connect to server manually in the desktop client
    * Auto-discovery does not work.  Cameras need to be added manually.

Macvlan
Info: https://docs.docker.com/v17.12/network/macvlan/

Licenses are set to the range specified in the macvlan network call.  If the IP changes, it invalidates the license unless you specify MAC address when running the container.

MAC address can be set in the docker run command with --mac-address="8a:ca:58:b9:e9:51" OR with docker compose mac_address: 8a:ca:58:b9:e9:51.

Pros:
    * Other systems on your network can see your server
    * You can see cameras on the network of your host
    * You can run multiple servers on your host
Cons:
    * Requires complicated configuration, including changes in DHCP server in your network and creating docker network. (Details here: https://docs.docker.com/v17.09/engine/userguide/networking/get-started-macvlan/)
    * Requires your network adapter to be in "Promiscuous" mode which can be a security risk. (Details here: https://searchsecurity.techtarget.com/definition/promiscuous-mode)

## Licenses ##

As stated in the networking section, licenses are tied to Hardware ID (HWID) and can be invalidated in a number of ways via modifying the Docker container:

    * Changes to the network settings can cause the HWID for example:
        * Changing a container from host to bridge
        * Starting up two containers in bridged mode and assigning license keys, stopping them, then starting them up in a different order.  This can be remedied by specifying MAC address
        * Deliberately changing the MAC address of a container
        * Changes to internal IP of the container can cause the MAC address to change as well
    * Moving the image to another computer
    * Some other cases may invalidate licenses, this is not a fully tested feature

Things that won't invalidate a license:

    * In-client update
    * Building a new image with a new server version but same MAC address and DB on the same host
    * Stopping and starting the container
    * Removing the container and starting it again from the same image and DB on the same host

NOTE: If your license has been invalidated, it can be reactivated up to 3 times by contacting support

## Software Updates ##

Both in-client and manual image updates will invalidate licenses.

In-client update:
Currently in-client update works but not perfectly.

    1.  Go to updates
    2.  Input version number and password then click ok
    3.  Click the download button
    4.  Once finished click the install button
    5.  The install will sit at "Installing..." for some time and then the "Finish Update" button will appear
    6.  Click the Finish Update button
    7.  Then click yes
    8.  You will now be at the Reconnecting... dialog.  This will also sit in this state.  Click cancel
    9.  This will close the connection to the server
    10. Stop your container
    11. Start it again
    12. Connect to it via the desktop client
    13. Open up to the in-client update page and you should get an update successful message
    14. You may be disconnected again but connecting once more should work and the updates tab should show the new version
    Note: At this point if you remove your container and start it again you will lose your update.  The following steps are for making your update permanent.
    15. Using the docker commit command make an image out of your current container. https://docs.docker.com/engine/reference/commandline/commit/
    16. Now you can stop and remove your current container.
    17. Docker run or compose and reference your new image.
    18. Server should be up and running with new version.

Updating the image itself:
    1. Bring down running container
    2. Move the copy of the DB and recorded video to another folder
    3. Remove container
    4. Remove image
    5. Build new image with different server version
    6. Bring DB and video files back
    7. Run the new image referencing the DB and video files
    8. Server should be up and running with new version

## MacOS support ##

With this Docker container you can run a container on MacOS!  It does come with some limitations.

    * "Host" networking will not work as the MacOS puts the Docker container inside a VM, not directly on the main OS.  https://docs.docker.com/docker-for-mac/networking/
    * "Bridge" is probably the best option but for the reasons above requires some manual addition of servers and cameras


## TODO ##

1. ~~Provide a better ways to specify deb file for mediaserver.~~ Done
1. Provide some scripts to simplify start&restart.
1. Interaction with cmake.
