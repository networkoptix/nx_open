Launches specified in settings client version.
If no such client version installed, downloads it, installs and launches...


Accepts tasks from clients and other launcher instances via named pipe.
Task has following format:

    {command name}\n
    {command parameter 1}\n
    {command parameter 2}\n
    ...
    {command parameter N}\n
    \n

For example, run client command look like:

    run\n
    1.4.3\n
    -e --log-level=DEBUG\n
    \n

Response has following format:
    
    {result code}\n
    {response parameter 1}\n
    {response parameter 2}\n
    ...
    {response parameter N}\n
    \n

Response example:

    ok\n
    \n
    
First line (required) - is a command. At the moment only "run" is supported.
Second line (required) - is a version to run
Third line (optinal) - command arguments


Installation files are downloaded one-by-one

Remote directory is defined by http url. E.g.: http://x.x.x.x/builds/networkoptix/2.1/. All directory/file names are relative to this directory

Mirror list is read from predefined http url (can be overridden by parameter mirrorListUrl).
Mirror list is defined by xml of following format:



<?xml version="1.0" encoding="UTF-8"?>
<condition resName="product" matchType="equal">

    <condition value="dwspectrum" resName="customization" matchType="equal">
        <condition value="digitalwatchdog" resName="platform" matchType="equal">
            <condition value="windows" resName="module" matchType="equal">
                <condition value="client" resName="version" matchType="equal">
                    <sequence value="1.5">
                        <set resName="mirrorUrl" resValue="http://dwserver1/1.5/"/>
                        <set resName="mirrorUrl" resValue="http://dwserver2/1.5/"/>
                    </sequence>
                    <sequence name="1.6">
                        <set resName="mirrorUrl" resValue="http://dwserver1/1.6/"/>
                        <set resName="mirrorUrl" resValue="http://dwserver2/1.6/"/>
                        <set resName="mirrorUrl" resValue="http://dwserverN/1.6/"/>
                    </sequence>
                </condition>
            </condition>
        </condition>
    </condition>

    <condition value="hdwitness" resName="customization" matchType="equal">
        <condition value="Vms" resName="module" matchType="equal">
            <condition value="client" resName="version" matchType="equal">
                <sequence value="1.5">
                    <set resName="mirrorUrl" resValue="http://networkoptix/archive/1.5/"/>
                    <set resName="mirrorUrl" resValue="http://networkoptix-mirror/1.5/"/>
                </sequence>
                <sequence name="1.6">
                    <set resName="mirrorUrl" resValue="http://networkoptix/archive/1.6/"/>
                </sequence>
            </condition>
        </condition>
    </condition>

</condition>


Contents of remote directory is read from contents.xml (url http://x.x.x.x/builds/networkoptix/2.1/contents.xml) file. File has following format:

<?xml version="1.0" encoding="UTF-8"?>
<contents validation="md5">
    <directory>client</directory>
    <directory>mediaserver</directory>
    <directory>appserver</directory>
    <file>help.hlp</file>
</contents>

- each directory (client, mediaserver, appserver) MUST have its own contents.xml

- optional validation="md5" attribute means that each file MAY be accompanied by *.md5 file (e.g., help.hlp.md5), which contains MD5 hash of file. 
    This information is used to skip downloading duplicate files
