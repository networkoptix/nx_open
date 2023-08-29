// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "highlighted_text_item_delegate.h"

#include <QtCore/QCache>
#include <QtCore/QSortFilterProxyModel>
#include <QtGui/QAbstractTextDocumentLayout>
#include <QtGui/QGuiApplication>
#include <QtGui/QPainter>
#include <QtWidgets/QAbstractItemView>

#include <nx/vms/client/core/skin/color_theme.h>
#include <nx/vms/client/desktop/style/helper.h>
#include <nx/vms/client/desktop/utils/widget_utils.h>
#include <nx/vms/common/html/html.h>

namespace {

struct CacheKey
{
    QString text;
    int width = 0;
    QColor color;
    QFont font;

    bool operator==(const CacheKey&) const = default;
};

static size_t qHash(const CacheKey& key, size_t seed = 0)
{
    return qHashMulti(seed, key.text, key.width, key.color.rgba(), key.font);
}

} // namespace

namespace nx::vms::client::desktop {

class HighlightedTextItemDelegate::Private
{
public:
    QSet<int> columns;

    QPointer<QAbstractItemView> view;
    mutable QCache<CacheKey, QPixmap> cache;

    Private(const QSet<int>& colums):
        columns(colums)
    {
    }

    QPixmap* getPixmap(const QString& text, const QStyleOptionViewItem& option) const
    {
        CacheKey key
        {
            .text = text,
            .width = option.rect.width(),
            .color = option.palette.color(QPalette::Text),
            .font = option.font
        };

        auto result = cache.object(key);

        if (!result && !text.isEmpty())
        {
            QTextDocument doc;

            doc.setDefaultFont(key.font);
            doc.setDocumentMargin(0);
            doc.setHtml(text);

            WidgetUtils::elideTextRight(&doc, key.width);

            const auto pixelRatio = qApp->devicePixelRatio();
            const auto pixmapSize = doc.size().toSize() * pixelRatio;

            QImage image(pixmapSize, QImage::Format_ARGB32_Premultiplied);

            if (image.isNull())
                return nullptr;

            image.setDevicePixelRatio(pixelRatio);
            image.fill(Qt::transparent);

            QPainter painter(&image);

            // Set default document color.
            QAbstractTextDocumentLayout::PaintContext ctx;
            ctx.palette.setColor(QPalette::Text, key.color);

            doc.documentLayout()->draw(&painter, ctx);

            result = new QPixmap(QPixmap::fromImage(image));

            if (!cache.insert(key, result))
                result = nullptr;
        }

        return result;
    }

    static QRegularExpression getSearchRx(const QStyleOptionViewItem& option)
    {
        if (const auto itemView = qobject_cast<const QAbstractItemView*>(option.widget))
        {
            if (const auto m = qobject_cast<const QSortFilterProxyModel*>(itemView->model()))
                return m->filterRegularExpression();
        }

        return {};
    }

    void updateCacheSize()
    {
        if (!view || !view->model())
            return;

        const int visibleRowCount = std::min(
            view->model()->rowCount(),
            view->size().height() / style::Metrics::kViewRowHeight);

        // Adjust cache to be twice the number of visible items.
        cache.setMaxCost(columns.count() * visibleRowCount * 2);
    }
};

HighlightedTextItemDelegate::HighlightedTextItemDelegate(
    QObject* parent,
    const QSet<int>& columns)
    :
    base_type(parent),
    d(new Private(columns))
{
    static const int kDefaultCachedRows = 20;
    d->cache.setMaxCost(columns.count() * kDefaultCachedRows);
}

HighlightedTextItemDelegate::~HighlightedTextItemDelegate()
{
}

QSize HighlightedTextItemDelegate::sizeHint(
    const QStyleOptionViewItem& option,
    const QModelIndex& index) const
{
    if (!d->columns.empty() && !d->columns.contains(index.column()))
        return base_type::sizeHint(option, index);

    auto opt = option;
    initStyleOption(&opt, index);

    QTextDocument doc;
    const auto text = common::html::highlightMatch(
        opt.text,
        Private::getSearchRx(opt),
        core::colorTheme()->color("yellow_d1"));

    doc.setDefaultFont(option.font);
    doc.setDocumentMargin(0);
    doc.setHtml(text);

    return QSize(
        qCeil(doc.idealWidth()) + 2 * style::Metrics::kStandardPadding,
        base_type::sizeHint(option, index).height());
}

void HighlightedTextItemDelegate::paint(
    QPainter* painter,
    const QStyleOptionViewItem& option,
    const QModelIndex& index) const
{
    if (!d->columns.empty() && !d->columns.contains(index.column()))
    {
        base_type::paint(painter, option, index);
        return;
    }

    auto opt = option;
    initStyleOption(&opt, index);

    if (opt.state.testFlag(QStyle::StateFlag::State_Selected))
        painter->fillRect(opt.rect, opt.palette.highlight());

    opt.rect = opt.rect.adjusted(
        style::Metrics::kStandardPadding, 0, -style::Metrics::kStandardPadding, 0);

    const auto text = common::html::highlightMatch(
        opt.text,
        Private::getSearchRx(opt),
        core::colorTheme()->color("yellow_d1"));

    if (QPixmap* pixmap = d->getPixmap(text, opt))
    {
        painter->save();

        const auto rect = QStyle::alignedRect(
            Qt::LeftToRight,
            Qt::AlignLeft | Qt::AlignVCenter,
            pixmap->deviceIndependentSize().toSize(),
            opt.rect);

        painter->drawPixmap(rect, *pixmap);

        painter->restore();
    }
}

bool HighlightedTextItemDelegate::eventFilter(QObject* object, QEvent* event)
{
    if (object == d->view && event->type() == QEvent::Resize)
        d->updateCacheSize();

    return false;
}

void HighlightedTextItemDelegate::setView(QAbstractItemView* view)
{
    if (!view->model())
        return;

    d->view = view;

    const auto updateCache = [this]() { d->updateCacheSize(); };

    connect(view->model(), &QAbstractItemModel::rowsInserted, this, updateCache);
    connect(view->model(), &QAbstractItemModel::rowsRemoved, this, updateCache);
    connect(view->model(), &QAbstractItemModel::modelReset, this, updateCache);

    view->installEventFilter(this);

    updateCache();
}

} // namespace nx::vms::client::desktop
