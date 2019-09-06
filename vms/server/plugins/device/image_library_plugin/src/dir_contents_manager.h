// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <map>
#include <string>

#include <camera/camera_plugin.h>

#include "mutex.h"

//!Manages information about image directory contents: Provides file list, generates timestamps of files, keep track of directory contents
class DirContentsManager
{
public:
    DirContentsManager(
        const std::string& imageDir,
        unsigned int frameDurationUsec );

    /*!
        \return map<timestamp, file full path>
    */
    std::map<nxcip::UsecUTCTimestamp, std::string> dirContents() const;
    void add( const nxcip::UsecUTCTimestamp& timestamp, const std::string& filePath );

    nxcip::UsecUTCTimestamp minTimestamp() const;
    nxcip::UsecUTCTimestamp maxTimestamp() const;

private:
    std::string m_imageDir;
    std::map<nxcip::UsecUTCTimestamp, std::string> m_dirContents;
    unsigned int m_frameDurationUsec;
    mutable Mutex m_mutex;

    void readDirContents();
};
