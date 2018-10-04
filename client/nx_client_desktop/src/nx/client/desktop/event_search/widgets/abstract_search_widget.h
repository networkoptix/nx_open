#pragma once

#include <QtCore/QScopedPointer>
#include <QtWidgets/QWidget>

#include <core/resource/resource_fwd.h>
#include <recording/time_period.h>
#include <ui/workbench/workbench_context_aware.h>

class QMenu;
class QLabel;
class QAction;
class QToolButton;

namespace nx::client::desktop {

class EventTile;
class SelectableTextButton;
class AbstractSearchListModel;

class AbstractSearchWidget:
    public QWidget,
    public QnWorkbenchContextAware
{
    Q_OBJECT
    using base_type = QWidget;

public:
    AbstractSearchWidget(
        QnWorkbenchContext* context,
        AbstractSearchListModel* model, //< Ownership is taken.
        QWidget* parent = nullptr);

    virtual ~AbstractSearchWidget() override;

    AbstractSearchListModel* model() const;

    enum class Control
    {
        cameraSelector = 0x01,
        timeSelector = 0x02,
        areaSelector = 0x04,
        freeTextFilter = 0x08,
        footersToggler = 0x10,
        previewsToggler = 0x20,

        defaults = cameraSelector | timeSelector | freeTextFilter | previewsToggler,
        all = defaults | areaSelector | footersToggler
    };
    Q_DECLARE_FLAGS(Controls, Control)

    Controls relevantControls() const;
    void setRelevantControls(Controls value);

    enum class Period
    {
        all, //< All the time.
        day, //< Last day.
        week, //< Last 7 days.
        month, //< Last 30 days.
        selection //< Timeline selection.
    };

    Period selectedPeriod() const;
    QnTimePeriod timePeriod() const;

    enum class Cameras
    {
        all, //< All cameras.
        current, //< Current camera.
        layout //< All cameras on current layout.
    };

    Cameras selectedCameras() const;
    QnVirtualCameraResourceSet cameras() const;

    QString textFilter() const;
    QRectF selectedArea() const;

    virtual void resetFilters();

signals:
    void tileHovered(const QModelIndex& index, const EventTile* tile); //< For external tooltip.
    void textFilterChanged(const QString& value);
    void timePeriodChanged(const QnTimePeriod& value);
    void cameraSetChanged(const QnVirtualCameraResourceSet& value);
    void selectedAreaChanged(const QRectF& value);

protected:
    void requestFetch();
    void setPlaceholderPixmap(const QPixmap& value);
    SelectableTextButton* createCustomFilterButton();

    QMenu* createMenu();
    void addDeviceDependentAction(
        QAction* action, const QString& mixedString, const QString& cameraString);

private:
    virtual QString placeholderText(bool constrained) const = 0;
    virtual QString itemCounterText(int count) const = 0;

protected:
    class Private;
    const QScopedPointer<Private> d;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(AbstractSearchWidget::Controls)

} // namespace nx::client::desktop
