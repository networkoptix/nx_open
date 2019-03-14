# Layout Files Structure {#layout_file_structure}

Both Nov Files and Executable Layouts have similar structure being discussed below.  
You may use [Nov Browser](#nov_browser) to display and export an actual Layout File.

## Nov File structure 

||||||
|-|-|-|-|-|
|Stream Index|(Crypto Info)|(Crypto) Stream 1|...|(Crypto) Stream N|

Any single Nov File may contain up to `kMaxStreams = 256` enclosed streams (probably actual number is 255, one less).  
In plain Nov Files, all Streams are not encrypted. In encrypted Nov Files, some streams are encrypted.

### Stream Index

Contains basic file info and list of streams. All structures in Nov Files are internally 4-byte aligned.  

~~~
struct StreamIndex
{
    quint64 magic = kIndexMagic;
    quint32 version = 1;
    quint32 entryCount = 0;
    std::array<StreamIndexEntry, kMaxStreams> entries = {};
};
~~~
Magic constant that starts the file is different for plain 

    quint64 kIndexMagic = 0xfed8260da9eebc04ll;

and encrypted files 
    
    quint64 kIndexCryptedMagic = 0xfed8260da9eebc03ll;

`StreamIndexEntry` structure that defines a single stream is defined as
~~~
struct StreamIndexEntry
{
    qint64 offset = 0;
    quint32 fileNameCrc = 0;
    quint32 reserved = 0;
};
~~~
`offset` is calculated from the first byte of file's StreamIndex, so it is essentially file offset for Nov Files, but it is not so for Executable Layouts (see below). `fileNameCrc` is used to   lookup streams by name. `reserved` is currently unused.

### Crypto Info

`CryptoInfo` is an optional structure in encrypted Nov File that only present if `StreamIndex.magic` is set to `kIndexCryptedMagic`:
~~~
struct CryptoInfo
{
    Key passwordSalt = {};
    Key passwordHash = {};
    std::array<unsigned char, 256 - 2 * kHashSize> reserved = {};
};
~~~
`passwordSalt` and `passwordHash` are 32-byte values that are used to check password that is used to open the file. Total size of CryptoInfo is set to 256 to accommodate any future updates.

#### Layout Streams
Each stream has the following structure:

||||
|-|-|-|
|Stream Name|(Crypto) Stream Contents||

Stream Name is a null-terminated string in utf-8 encoding. This field contains arbitrary number of bytes less than 1024. Stream Contents for plain layout files is just plain byte stream with  arbitrary size. Stream Contents may 

#### Crypto Streams Contents
Crypto Streams Contents is an independent container that use password-driven encryption.
The container size is always divided into fixed-sized 1024-byte blocks. First block contains container header (padded to 1024 bytes) and next blocks contain encrypted data.
||||
|-|-|-|
|Header|Header Padding|Contents|

Header contains information about container version, minimal reader version to read this container, size of unencrypted data and encryption information. `salt` is used both to check the password and also to construct the encryption key, and `keyHash` is used to check the password.


~~~
struct Header
{
    qint64 version = kCryptoStreamVersion;
    qint64 minReadVersion = kCryptoStreamVersion; //< Minimal reader version to access the stream.
    qint64 dataSize = 0;
    Key salt = {};
    Key keyHash = {};
} m_header;
~~~

## Executable Layouts

Executable Layout consists of Launcher, NX Witness Client, Nov Container and tail bytes: 

|||||
|-|-|-|-|
|Launcher|NX Witness Client Files|Nov Container|Tail|

Launcher is a Windows executable that unpacks NX Witness Client and launches NW Client, passing Executable Layout filename via command line. NX Witness Client starts and opens Executable Layout directly as if it were a Nov File.
NX Witness Client Files are taken from current NW client installation when creating Executable Layout.
Nov Container is exactly equivalent to the Nov File. It is important that internal file offset values used in Nov Container are relative to the start of the container.
Tail consists of two 8-byte values. First is offset of the Nov Container relative to the start of the file. Second is magic constant `kFileMagic = 0x73a0b934820d4055ull`  that is used to check the file for consistency by Launcher.
