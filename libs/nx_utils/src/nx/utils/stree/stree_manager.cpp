// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "stree_manager.h"

#include <QtCore/QFile>
#include <QtCore/QVariant>
#include <QtCore/QXmlStreamReader>

#include <nx/utils/log/log.h>
#include <nx/utils/serialization/qt_xml_helper.h>

#include "streesaxhandler.h"

namespace nx::utils::stree {

StreeManager::StreeManager(const std::string& xmlFilePath) noexcept(false):
    m_xmlFilePath(xmlFilePath)
{
    loadStree();
}

void StreeManager::search(
    const nx::utils::stree::AbstractAttributeReader& input,
    nx::utils::stree::AbstractAttributeWriter* const output) const
{
    m_stree->get(input, output);
}

std::unique_ptr<nx::utils::stree::AbstractNode> StreeManager::loadStree(const nx::Buffer& data)
{
    nx::utils::stree::SaxHandler xmlHandler;

    QXmlStreamReader reader(data.toRawByteArray());
    if (!nx::utils::parseXml(reader, xmlHandler))
    {
        NX_WARNING(typeid(StreeManager), "Failed to parse stree xml: %1. %2",
            reader.errorString(), xmlHandler.errorString());
        return nullptr;
    }

    return xmlHandler.releaseTree();
}

void StreeManager::loadStree() noexcept(false)
{
    QFile xmlFile(QString::fromStdString(m_xmlFilePath));
    if (!xmlFile.open(QIODevice::ReadOnly))
    {
        throw std::runtime_error("Failed to open stree xml file " +
            m_xmlFilePath + ": " + xmlFile.errorString().toStdString());
    }

    NX_DEBUG(this, "Parsing stree xml file (%1)", m_xmlFilePath);
    m_stree = loadStree(xmlFile.readAll());
    if (!m_stree)
        throw std::runtime_error("Failed to parse stree xml file " + m_xmlFilePath);
}

} // namespace nx::utils::stree
