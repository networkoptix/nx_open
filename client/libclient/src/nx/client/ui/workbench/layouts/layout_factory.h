#pragma once

#include <QtCore/QObject>

#include <core/resource/resource_fwd.h>
#include <common/common_globals.h>
#include <nx/utils/singleton.h>

class QnWorkbenchLayout;

namespace nx {
namespace client {
namespace ui {
namespace workbench {
namespace layouts {

class LayoutsFactory:
    public QObject,
    public Singleton<LayoutsFactory>
{
    Q_OBJECT
    using base_type = QObject;

public:
    using LayoutCreator =
        std::function<QnWorkbenchLayout* (const QnLayoutResourcePtr& resource, QObject* parent)>;

    LayoutsFactory(QObject* parent = nullptr);

    QnWorkbenchLayout* create(QObject* parent = nullptr);

    QnWorkbenchLayout* create(
        const QnLayoutResourcePtr& resource,
        QObject* parent = nullptr);

    void addCreator(const LayoutCreator& creator);

private:
    using CreatorsList = QList<LayoutCreator>;
    CreatorsList m_creators;
};

} // namespace layouts
} // namespace workbench
} // namespace ui
} // namespace client
} // namespace nx

#define qnLayoutFactory nx::client::ui::workbench::layouts::LayoutsFactory::instance()
