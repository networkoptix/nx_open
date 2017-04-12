# time_server - Time Syncronization Server (RFC 868)

## Build

```
$ mvn package -Dbuild.configuration=release -pl nx_cloud/time_server
$ ./nx_cloud/time_server/scripts/create_package.sh
$ cp time_server.tar.gz ...
```

## Install & Start

```
$ scp time_server.tar.gz $SERVER:.
$ ssh $SERVER sudo bash
> tar zxvf time_server.tar.gz -C /
> systemctl enable nx-time-server # Ubuntu 16+ only
> service nx-time-server start
```

## Test it's working

```
$ netcat $SERVER 37 | xdd
```
