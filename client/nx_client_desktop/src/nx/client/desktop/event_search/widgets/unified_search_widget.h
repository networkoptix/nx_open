#pragma once

#include <QtCore/QPointer>
#include <QtCore/QScopedPointer>
#include <QtCore/QTimer>
#include <QtWidgets/QWidget>

#include <recording/time_period.h>
#include <ui/workbench/workbench_context_aware.h>

class QAbstractListModel;
class QnSearchLineEdit;
class QLabel;
class QMenu;
class QToolButton;
class QnDisconnectHelper;

namespace Ui { class UnifiedSearchWidget; }

namespace nx {

namespace utils { class PendingOperation; }

namespace client {
namespace desktop {

namespace ui { class SelectableTextButton; }

class UnifiedAsyncSearchListModel;
class EventTile;

class UnifiedSearchWidget:
    public QWidget,
    public QnWorkbenchContextAware
{
    Q_OBJECT
    using base_type = QWidget;

public:
    UnifiedSearchWidget(QWidget* parent);
    virtual ~UnifiedSearchWidget() override;

    QAbstractListModel* model() const;
    void setModel(QAbstractListModel* value);

    QnSearchLineEdit* filterEdit() const;
    ui::SelectableTextButton* typeButton() const;
    ui::SelectableTextButton* areaButton() const;
    ui::SelectableTextButton* timeButton() const;
    ui::SelectableTextButton* cameraButton() const;

    QToolButton* showInfoButton() const;
    QToolButton* showPreviewsButton() const;

    QLabel* counterLabel() const;

    void setPlaceholderIcon(const QPixmap& value);
    void setPlaceholderTexts(const QString& constrained, const QString& unconstrained);

    enum class Period
    {
        all, //< All the time.
        day, //< Last day.
        week, //< Last 7 days.
        month, //< Last 30 days.
        selection //< Timeline selection.
    };

    Period selectedPeriod() const;
    void setSelectedPeriod(Period value);

    QnTimePeriod currentTimePeriod() const;

    void requestFetch();

signals:
    void tileHovered(const QModelIndex& index, const EventTile* tile);
    void currentTimePeriodChanged(const QnTimePeriod& period);

protected:
    virtual bool hasRelevantTiles() const;
    virtual bool setCurrentTimePeriod(const QnTimePeriod& period);
    virtual bool isConstrained() const;

private:
    void updateCurrentTimePeriod();
    QnTimePeriod effectiveTimePeriod() const;

    void setupTimeSelection();
    void updatePlaceholderState();

    void fetchMoreIfNeeded();

private:
    QScopedPointer<Ui::UnifiedSearchWidget> ui;
    QnSearchLineEdit* m_searchLineEdit;
    QTimer* const m_dayChangeTimer = nullptr;
    nx::utils::PendingOperation* m_fetchMoreOperation;
    QnTimePeriod m_timelineSelection;
    Period m_period = Period::all;
    QString m_placeholderTextConstrained;
    QString m_placeholderTextUnconstrained;
    QnTimePeriod m_currentTimePeriod = QnTimePeriod::anytime();
    QScopedPointer<QnDisconnectHelper> m_modelConnections;
};

} // namespace desktop
} // namespace client
} // namespace nx
