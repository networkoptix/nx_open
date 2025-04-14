// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "malloc_info.h"

#ifdef __linux__
#include <malloc.h>
#include <stdio.h>
#endif

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/xml_parser.hpp>

#include <nx/utils/log/log.h>

namespace nx::utils::memory {

namespace {

void parseMemoryStat(const boost::property_tree::ptree& tree, MallocInfo::MemoryStat* out)
{
    out->type = tree.get<std::string>("<xmlattr>.type");
    if (auto count = tree.get_optional<size_t>("<xmlattr>.count"))
        out->count = *count;
    out->size = tree.get<size_t>("<xmlattr>.size");
}

void parseHeap(const boost::property_tree::ptree& tree, MallocInfo::Heap* out)
{
    out->nr = tree.get<size_t>("<xmlattr>.nr", 0);

    for (const auto& [tag, child] : tree)
    {
        if (tag == "sizes")
        {
            for (const auto& [type, attrs]: child)
            {
                MallocInfo::Heap::Sizes::Size s {
                    .from = attrs.get<std::size_t>("<xmlattr>.from"),
                    .to = attrs.get<std::size_t>("<xmlattr>.to"),
                    .total = attrs.get<std::size_t>("<xmlattr>.total"),
                    .count = attrs.get<std::size_t>("<xmlattr>.count"),
                };

                if (type == "size")
                    out->sizes.size.push_back(s);
                else if (type == "unsorted")
                    out->sizes.unsorted = s;
            }
        }
        else if (tag == "total")
        {
            parseMemoryStat(child, &out->total.emplace_back());
        }
        else if(tag == "system")
        {
            parseMemoryStat(child, &out->system.emplace_back());
        }
        else if(tag == "aspace")
        {
            parseMemoryStat(child, &out->aspace.emplace_back());
        }
    }
}

void parseMallocInfo(const boost::property_tree::ptree& tree, MallocInfo* out)
{
    auto& mallocNode = tree.get_child("malloc");
    out->version = mallocNode.get<size_t>("<xmlattr>.version");

    for (const auto& [tag, child]: mallocNode)
    {
        if (tag == "heap")
            parseHeap(child, &out->heap.emplace_back());
        else if (tag == "total")
            parseMemoryStat(child, &out->total.emplace_back());
        else if(tag == "system")
            parseMemoryStat(child, &out->system.emplace_back());
        else if(tag == "aspace")
            parseMemoryStat(child, &out->aspace.emplace_back());
    }
}

} // namespace

bool mallocInfo(
    std::string* data,
    std::string* contentType)
{
#if defined(__linux__) && !defined(__ANDROID__)
    char* buf = nullptr;
    std::size_t bufSize = 0;
    FILE* outputStream = open_memstream(&buf, &bufSize);
    if (outputStream == nullptr)
        return false;

    if (malloc_info(0 /*options*/, outputStream) != 0)
    {
        fclose(outputStream);
        free(buf);
        return false;
    }

    fflush(outputStream);
    fclose(outputStream);

    if (buf == nullptr)
        return false;

    *contentType = "text/xml";
    *data = buf;

    free(buf);

    return true;
#else
    *contentType = "text/plain";
    *data = "not implemented";

    return true;
#endif
}

MallocInfo fromXml(const std::string& xml)
{
    try
    {
        std::istringstream iss{xml};
        boost::property_tree::ptree pt;
        read_xml(iss, pt, boost::property_tree::xml_parser::no_comments);

        MallocInfo result;
        parseMallocInfo(pt, &result);

        return result;
    }
    catch(const std::exception& e)
    {
        NX_WARNING(NX_SCOPE_TAG, "Failed to deserialize malloc_info xml: %1", e.what());
        return {};
    }
}

} // namespace nx::utils::memory
