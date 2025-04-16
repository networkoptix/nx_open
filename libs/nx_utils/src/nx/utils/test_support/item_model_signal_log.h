// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <functional>

#include <QtCore/QObject>
#include <QtCore/QPointer>
#include <QtCore/QStringList>

#include <nx/utils/impl_ptr.h>

class QAbstractItemModel;

namespace nx::utils::test {

/**
 * An utility object for unit tests. It connects to specified item model's signals and stores
 * information about their invocations in a text form. Each signal invocation yields a single text
 * line with the signal name, arguments and, optionally, current row or column count, whichever is
 * relevant for the signal.
 *
 * Arguments are logged in parentheses following the signal name, they are separated by comma ","
 * without spaces. A signal without arguments is followed by empty parentheses "()".
 *
 * A valid top level index is logged as a pair {row}:{column}, for example "10:2". If OmitColumns
 * option is specified and the column is zero, only the row is logged, for example "5".
 *
 * A valid child index is prefixed with a chain of parents separated by "/",
 * for example "5:0/1:0/0:8", or, if OmitColumns option is specified, "5/1/0:8".
 *
 * An invalid index is logged as "empty".
 *
 * Arrays are logged in square brackets, with items separated by comma "," without spaces.
 * Examples: "[2,3]", "[]" (an empty array).
 */
class NX_UTILS_API ItemModelSignalLog: public QObject
{
    Q_OBJECT

public:
    enum Option
    {
        /**
         * Omit empty parent indexes where possible. Useful for list and table models. If this
         * option is specified, row/column insert/remove/move signals for operations on the top
         * level of the model hierarchy are logged without parent/sourceParent/destinationParent
         * arguments altogether.
         *
         * Examples:
         *     "columnsInserted(5,6)" instead of "columnsInserted(empty,5,6)"
         *     "rowsAboutToBeMoved(6,10,0)" instead of "rowsAboutToBeMoved(empty,6,10,empty,0)".
         */
        OmitParents = 0x1,

        /**
         * Omit zero column indexes where possible. Useful for list and single column tree models.
         * If this option is specified, indexes with column=0 are logged as a single row.
         *
         * Examples:
         *     "dataChanged(1,9,[])" instead of "dataChanged(1:0,9:0,[])"
         *     "rowsInserted(4/9/0,7,1)" instead of "rowsInserted(4:0/9:0/0:0,7,1)"
         */
        OmitColumns = 0x2,

        /**
         * Add current row or column count (whichever is relevant for the signal)
         * at the end of a simple signal entry after a " |" separator.
         * For move signals between different parents both source parent and destination parent
         * counts are appended.
         *
         * Counts are not logged for model resets, data changes nor layout changes.
         *
         * Examples:
         *     "columnsInserted(empty,3,9) |10".
         *     "rowsAboutToBeMoved(0/0,3,5,0/1,0) |6 |0".
         */
        AppendCounts = 0x4
    };
    Q_DECLARE_FLAGS(Options, Option);

public:
    /** Construct ItemModelSignalLog with specified options. */
    explicit ItemModelSignalLog(
        QAbstractItemModel* model, Options options, QObject* parent = nullptr);

    /**
     * Construct ItemModelSignalLog with default options based on a model type.
     * For QAbstractListModel descendants: OmitParents | OmitColumns | AppendCounts
     * For QAbstractTableModel descendants: OmitParents | AppendCounts
     * For all other models: OmitColumns | AppendCounts
     */
    explicit ItemModelSignalLog(QAbstractItemModel* model, QObject* parent = nullptr);

    virtual ~ItemModelSignalLog() override;

    /** Get current signal log. */
    QStringList log() const;

    /** Clear signal log. */
    void clear();

    /**
     * Add custom entry to the signal log.
     * Can be useful if a model descendant emits a custom signal
     */
    void addCustomEntry(const QString& entry);

    QAbstractItemModel* model() const;
    Options options() const;

private:
    struct Private;
    ImplPtr<Private> d;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(ItemModelSignalLog::Options)

} // namespace nx::utils::test
