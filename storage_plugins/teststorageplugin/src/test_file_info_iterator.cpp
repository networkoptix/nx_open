#include <cstring>
#include "test_file_info_iterator.h"

TestFileInfoIterator::TestFileInfoIterator(FsStubNode* root)
    : m_cur(root->child)
{}

nx_spl::FileInfo* STORAGE_METHOD_CALL TestFileInfoIterator::next(int* ecode) const
{
    auto resultNode = m_cur;
    if (m_cur)
        m_cur = m_cur->next;

    m_fInfo.url = resultNode->name;
    m_fInfo.type = resultNode->type == file ? nx_spl::isFile : nx_spl::isDir;
    m_fInfo.size = resultNode->size;

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


