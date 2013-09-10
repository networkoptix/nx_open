/**********************************************************
* 04 sep 2013
* akolesnikov@networkoptix.com
***********************************************************/

#ifndef DIR_CONTENTS_MANAGER_H
#define DIR_CONTENTS_MANAGER_H

#include <map>
#include <string>

#include <plugins/camera_plugin.h>

//#include "mutex.h"


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

    nxcip::UsecUTCTimestamp minTimestamp() const;
    nxcip::UsecUTCTimestamp maxTimestamp() const;

private:
    std::string m_imageDir;
    std::map<nxcip::UsecUTCTimestamp, std::string> m_dirContents;
    unsigned int m_frameDurationUsec;
    //mutable Mutex m_mutex;

    void readDirContents();
};

#endif  //DIR_CONTENTS_MANAGER_H
