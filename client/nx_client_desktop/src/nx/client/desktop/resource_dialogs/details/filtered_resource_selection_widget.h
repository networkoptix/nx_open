#pragma once

#include <QtWidgets/QScrollArea>

namespace Ui { class FilteredResourceSelectionWidget; }

namespace nx::client::desktop {

namespace node_view { class ResourceSelectionNodeView; }

class FilteredResourceSelectionWidget: public QScrollArea
{
    Q_OBJECT
    using base_type = QScrollArea;

public:
    FilteredResourceSelectionWidget(QWidget* parent = nullptr);
    virtual ~FilteredResourceSelectionWidget() override;

    node_view::ResourceSelectionNodeView* resourceSelectionView();

private:
    const QScopedPointer<Ui::FilteredResourceSelectionWidget> ui;
};

} // namespace nx::client::desktop
