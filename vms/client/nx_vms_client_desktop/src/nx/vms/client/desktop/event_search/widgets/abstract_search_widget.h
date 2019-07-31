#pragma once

#include <QtWidgets/QWidget>

#include <core/resource/resource_fwd.h>
#include <recording/time_period.h>
#include <ui/workbench/workbench_context_aware.h>

#include <nx/utils/impl_ptr.h>

class QMenu;
class QLabel;
class QAction;
class QToolButton;

namespace nx::vms::client::desktop {

class EventTile;
class EventRibbon;
class SelectableTextButton;
class AbstractSearchListModel;

/**
 * Base class for Right Panel search tabs. Contains common filter setup controls, tiles view
 * (also known as EventRibbon) to display data and all mechanics to fetch data when needed.
 */
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

    /** Whether the search widget should be visible. */
    bool isAllowed() const;

    /** Rewind to live. Discards loaded archive if neccessary. */
    virtual void goToLive();

    /** Relevant controls in the tab. Irrelevant controls are hidden. */
    enum class Control
    {
        cameraSelector = 0x01,
        timeSelector = 0x02,
        freeTextFilter = 0x04,
        footersToggler = 0x08,
        previewsToggler = 0x10,

        defaults = cameraSelector | timeSelector | freeTextFilter | previewsToggler,
        all = defaults | footersToggler
    };
    Q_DECLARE_FLAGS(Controls, Control)

    Controls relevantControls() const;
    void setRelevantControls(Controls value);

    /** User-selectable relevant time period options. */
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

    /** User-selectable relevant camera set options. */
    enum class Cameras
    {
        all, //< All cameras.
        current, //< Current camera.
        layout //< All cameras on current layout.
    };

    Cameras selectedCameras() const;
    QnVirtualCameraResourceSet cameras() const;

    void selectCameras(Cameras value);
    Cameras previousCameras() const;

    QString textFilter() const;

    /**
     * Resets all filters to their default state.
     * Derived classes that add their own filters must override this method.
     */
    virtual void resetFilters();

signals:
    /** This signal is for displaying external tooltips. */
    void tileHovered(const QModelIndex& index, EventTile* tile, QPrivateSignal);

    /** This signal is sent when current camera selection type or actual camera set changes. */
    void cameraSetChanged(const QnVirtualCameraResourceSet& value, QPrivateSignal);

    void textFilterChanged(const QString& value, QPrivateSignal);
    void timePeriodChanged(const QnTimePeriod& value, QPrivateSignal);

    bool allowanceChanged(bool value, QPrivateSignal);

protected:
    /** Sets an icon for no data placeholder. */
    void setPlaceholderPixmap(const QPixmap& value);

    /** Allows derived classes to create own filter buttons added to the end of the list. */
    SelectableTextButton* createCustomFilterButton();

    /** Creates a child menu with dropdown appearance. */
    QMenu* createDropdownMenu();

    /** Setup bypass graphics proxy widget for the menu. */
    static void fixMenuFlags(QMenu* menu);

    /**
     * Adds specified action to the list of device dependent actions.
     * When connection state becomes "Ready" (initial resources are received) all device dependent
     * action texts are updated to either mixedString or cameraString depending on device types
     * present in the system.
     */
    void addDeviceDependentAction(
        QAction* action, const QString& mixedString, const QString& cameraString);

    /** Internal access to tiles view. */
    EventRibbon* view() const;

    /** Should be called from descendants when isAllowed should be recalculated. */
    void updateAllowance();

private:
    /**
     * Derived classes must override this method and return localized string for
     * no data placeholder. When constrained is true it means there's no data matching
     * search criteria, when constrained is false it means there's no data in the system at all.
     */
    virtual QString placeholderText(bool constrained) const = 0;

    /**
     * Derived classes must override this method and return localized string for item counter.
     */
    virtual QString itemCounterText(int count) const = 0;

    /** Derived classes should override this method if the widget is not always allowed. */
    virtual bool calculateAllowance() const { return true; }

private:
    class Private;
    nx::utils::ImplPtr<Private> d;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(AbstractSearchWidget::Controls)

} // namespace nx::vms::client::desktop
