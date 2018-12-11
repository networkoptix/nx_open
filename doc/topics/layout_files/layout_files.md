# Layout Files Handling {#layout_files}

## Introduction
NX Witness client has its own binary format for saving layouts together with video streams, images and various metadata.
For macOS and Linux clients, the saved files have ".nov" extension. For Windows, also self-unpacking executable (".exe") files containing basic distribution of  NX Witness client together with video data are available. These packages can replay saved layout on a Windows system that originally contains no NX Client distribution.
We will use the term "Nov File" for ".nov" plain layouts and "Executable Layout" for ".exe" self-executable unpacking layouts.

Both Nov Files and Executable Layouts may use optional password to protect the contents. Video and image streams in such files are encrypted and cannot be viewed without entering the password.

This document focuses on technical implementation of saved layout files in NX Witness client. 

@subpage nov_browser

@subpage layout_file_structure

@subpage layout_classes


## Things you should know when working with Layout Files
There are some things important to know if you work with Layout files:

### Layout Files Properties
- Only some streams (currently video and images) are encrypted in password-protected Layout Fules. Discrimination is done by `QnLayoutFileStorageResource::shouldCrypt`. Others are in plain format.

### Examining Layout File
- To figure out if .nov or .exe file is a Layout File, use @ref nx::core::layout::identifyFile.
- To check a password for the Layout File, use @ref nx::core::layout::checkPassword.

### Working with Layout File
- Both read and write operations are provided by @ref QnLayoutFileStorageResource class.
- Set file name with `QnLayoutFileStorageResource::setUrl()`. 
- Use `QnLayoutFileStorageResource::open()` to find or create a specific stream.
- Only one stream can be opened for write operation because all stream lay in the file one-by-one.
- When writing a Layout File, it is always written to .tmp file first. This is done to prevent premature scan by NW Client resource explorer as well as possible antivirus software invocation for Executable Layouts.
- Many @ref QnLayoutFileStorageResource objects may reference the same Nov File. This is used in NW Client and should be safe until only read access is used.
- Renaming or moving of Nov File is done this way (`QnLayoutFileStorageResource::switchToFile()`)
    - All active streams are "hung" on mutexes and their state is saved.
    - The file is renamed.
    - Active streams are re-opened and mutexes are released.