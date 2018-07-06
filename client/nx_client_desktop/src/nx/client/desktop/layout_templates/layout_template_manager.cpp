#include "layout_template_manager.h"

#include <QtCore/QStandardPaths>
#include <QtCore/QDir>
#include <QtCore/QFile>

#include <nx/fusion/model_functions.h>

namespace nx {
namespace client {
namespace desktop {

namespace {

static const QString kTemplatesDirName = lit("layout_templates");
static constexpr qint64 kMaxFileSize = 10 * 1024 * 1024;

} // namespace

//-------------------------------------------------------------------------------------------------

class LayoutTemplateManager::Private
{
public:
    QList<QString> templateDirectories;
    QList<LayoutTemplate> templates;

public:
    void loadTemplatesFromDirectory(const QString& path);
};

void LayoutTemplateManager::Private::loadTemplatesFromDirectory(const QString& path)
{
    const QDir dir(path);

    for (const auto& entry: dir.entryList({lit("*.json")}, QDir::Files))
    {
        const auto& fileName = dir.absoluteFilePath(entry);

        QFile file(fileName);
        if (!file.open(QFile::ReadOnly))
            continue;

        if (file.size() > kMaxFileSize)
            continue;

        const QByteArray data = file.readAll();

        const auto& layoutTemplate = QJson::deserialized<LayoutTemplate>(data);
        if (layoutTemplate.isValid())
            templates.append(layoutTemplate);
    }
}

//-------------------------------------------------------------------------------------------------

LayoutTemplateManager::LayoutTemplateManager(QObject* parent):
    base_type(parent),
    d(new Private)
{
    d->templateDirectories.append(
        QDir(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation))
            .absoluteFilePath(kTemplatesDirName));

    reloadTemplates();
}

LayoutTemplateManager::~LayoutTemplateManager()
{
}

QList<QString> LayoutTemplateManager::templateDirectories() const
{
    return d->templateDirectories;
}

void LayoutTemplateManager::setTemplateDirectories(const QList<QString>& directories)
{
    d->templateDirectories = directories;
}

QList<LayoutTemplate> LayoutTemplateManager::templates() const
{
    return d->templates;
}

void LayoutTemplateManager::reloadTemplates()
{
    d->templates.clear();

    for (const auto& directory: d->templateDirectories)
        d->loadTemplatesFromDirectory(directory);
}

} // namespace desktop
} // namespace client
} // namespace nx
