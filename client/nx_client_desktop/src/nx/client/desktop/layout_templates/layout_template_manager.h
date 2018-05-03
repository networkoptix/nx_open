#pragma once

#include <QtCore/QObject>

#include "layout_template.h"

namespace nx {
namespace client {
namespace desktop {

class LayoutTemplateManager: public QObject
{
    Q_OBJECT

    using base_type = QObject;

public:
    LayoutTemplateManager(QObject* parent = nullptr);
    virtual ~LayoutTemplateManager() override;

    QList<QString> templateDirectories() const;
    void setTemplateDirectories(const QList<QString>& directories);

    QList<LayoutTemplate> templates() const;

    void reloadTemplates();

private:
    class Private;
    QScopedPointer<Private> d;
};

} // namespace desktop
} // namespace client
} // namespace nx
