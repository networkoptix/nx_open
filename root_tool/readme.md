# Root Tool
Root tool is a supplementary standalone linux service run under control of the Service Manager
(systemd, upstart) which is used by Nx Witness Mediaserver to perform certain operations which
require superuser access such as:
* Mounting/unmounting external cifs storages
* Acquiring hardware information for license-related operations
* Reading and writing database and media files on already mounted by other OS users partitions

## Supported commands
|Command|Description|
|-------|-----------|
|`mount`|mount cifs file system|
|`umount`|umount mounted cifs file system|
|`chown`|change ownership for certain path in the file system|
|`mkdir`|create directory with the given path|
|`rm`|remove given path|
|`mv`|move/rename file from one location to another|
|`open`|open file and return file descriptor|
|`freeSpace`|return free space number for the device on which the given path is located|
|`totalSpace`|return total space number for the device on which the given path is located|
|`exists`|return whether the given path exists|
|`list`|return serialized contents of the given directory|
|`dmiInfo`|return certain hardware information necessary for the hardware ID calculation|

## Components
<img src="doc/Root tool components.png"></img>

## When Root Tool is used
<img src="doc/When use root tool.png"></img>

When Root Tool is not used, Mediaserver uses `SystemCommands` library (see above) directly.

## Workflow
<img src="doc/Root tool sequence.png"></img>

## Protocol
Mediaserver and Root Tool communicate over Unix Domain Socket using following binary protocol

|Field|Size (bytes)|Description|
|-----|------------|-----------|
|`length`|4|length of the following command string|
|`command`|variable|command string|