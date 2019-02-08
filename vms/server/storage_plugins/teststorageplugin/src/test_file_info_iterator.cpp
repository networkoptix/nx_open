#include <cstring>
#include "test_file_info_iterator.h"

namespace {

}

TestFileInfoIterator::TestFileInfoIterator(FsStubNode* root, const std::string& prefix)
    : m_cur(root->child),
      m_prefix(prefix)
{}

nx_spl::FileInfo* STORAGE_METHOD_CALL TestFileInfoIterator::next(int* ecode) const
{
    if (m_cur == nullptr)
        return nullptr;

    auto resultNode = m_cur;
    if (m_cur)
        m_cur = m_cur->next;

    char buf[4096];
    FsStubNode_fullPath(resultNode, buf, sizeof buf);
    memset(m_urlBuf, '\0', sizeof m_urlBuf);
    strcpy(m_urlBuf, m_prefix.data());
    strcat(m_urlBuf, buf);

    m_fInfo.url = m_urlBuf;
    m_fInfo.type = resultNode->type == file ? nx_spl::isFile : nx_spl::isDir;
    m_fInfo.size = resultNode->size;

    if (ecode)
        *ecode = nx_spl::error::NoError;

    return &m_fInfo;
}

void* TestFileInfoIterator::queryInterface(const nxpl::NX_GUID& interfaceID) 
{
    if (std::memcmp(&interfaceID,
                    &nx_spl::IID_FileInfoIterator,
                    sizeof(nxpl::NX_GUID)) == 0)
    {
        addRef();
        return static_cast<nx_spl::FileInfoIterator*>(this);
    }
    else if (std::memcmp(&interfaceID,
                         &nxpl::IID_PluginInterface,
                         sizeof(nxpl::IID_PluginInterface)) == 0)
    {
        addRef();
        return static_cast<nxpl::PluginInterface*>(this);
    }
    return nullptr;

}

unsigned int TestFileInfoIterator::addRef() 
{
    return pAddRef();
}

unsigned int TestFileInfoIterator::releaseRef() 
{
    return pReleaseRef();
}


