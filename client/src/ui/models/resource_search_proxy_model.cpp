#include "resource_search_proxy_model.h"
#include "resource_pool_model.h"

#include <utils/common/delete_later.h>
#include <utils/network/nettools.h>

#include <core/resource_management/resource_criterion.h>
#include <core/resource/resource.h>

#include <client/client_globals.h>

QnResourceSearchProxyModel::QnResourceSearchProxyModel(QObject *parent): 
    QSortFilterProxyModel(parent),
    m_invalidating(false)
{
    // TODO: #Elric use natural string comparison instead.
    setSortCaseSensitivity(Qt::CaseInsensitive);
}

QnResourceSearchProxyModel::~QnResourceSearchProxyModel() {
    return;
}

void QnResourceSearchProxyModel::addCriterion(const QnResourceCriterion &criterion) {
    m_criterionGroup.addCriterion(criterion);

    invalidateFilterLater();
}

bool QnResourceSearchProxyModel::removeCriterion(const QnResourceCriterion &criterion) {
    bool removed = m_criterionGroup.removeCriterion(criterion);
    if(removed)
        invalidateFilterLater();

    return removed;
}

void QnResourceSearchProxyModel::clearCriteria() {
    if(m_criterionGroup.empty())
        return;

    m_criterionGroup.clear();
    invalidateFilterLater();
}

QnResourcePtr QnResourceSearchProxyModel::resource(const QModelIndex &index) const {
    return data(index, Qn::ResourceRole).value<QnResourcePtr>();
}

void QnResourceSearchProxyModel::invalidateFilter() {
    m_invalidating = false;
    QSortFilterProxyModel::invalidateFilter();
    emit criteriaChanged();
}

void QnResourceSearchProxyModel::invalidateFilterLater() {
    if(m_invalidating)
        return; /* Already waiting for invalidation. */

    m_invalidating = true;
    QMetaObject::invokeMethod(this, "invalidateFilter", Qt::QueuedConnection);
}

bool QnResourceSearchProxyModel::filterAcceptsRow(int source_row, const QModelIndex &source_parent) const {
    QModelIndex index = sourceModel()->index(source_row, 0, source_parent);
    if(!index.isValid())
        return true;

    Qn::NodeType nodeType = index.data(Qn::NodeTypeRole).value<Qn::NodeType>();
    if(nodeType == Qn::UsersNode)
        return false; /* We don't want users in search. */

    if(nodeType == Qn::RecorderNode) {
        for (int i = 0; i < sourceModel()->rowCount(index); i++) {
            if (filterAcceptsRow(i, index))
                return true;
        }
        return false;
    }

    QnResourcePtr resource = index.data(Qn::ResourceRole).value<QnResourcePtr>();
    if(resource.isNull())
        return true;

    QnResourceCriterion::Operation operation = m_criterionGroup.check(resource);
    if(operation != QnResourceCriterion::Accept)
        return false;

    return true;
}


namespace {

static const std::size_t LOOK_UP_TABLE_SIZE = 58;

// A look up table to speed up comparison for just English alpha word
static const int kLookUpTable[LOOK_UP_TABLE_SIZE] = {
    1,3,5,7,9,11,13,15,17,19,21,23,25,27,29,31,33,35,37,39,41,43,45,47,49,51, // end of the range: A-Z
    0,0,0,0,0,0, // 6 none alpha characters
    2,4,6,8,10,12,14,16,18,20,22,24,26,28,30,32,34,36,38,40,42,44,46,48,50,52 // end of the range: a-z
};

#define LOOK_UP_TABLE_INDEX(c) ((c)-'A')

// We cannot use QChar::isLetter since we only care about English word , additionally
// std::isalpha is still not the right choice since it is affected by the local page
inline bool isValidComparableAlpha( int cha ) {
    return (cha > 'A'-1 && cha < 'Z'+1) || (cha >'a'-1 && cha <'z'+1);
}

bool stringLessThan( const QString& left , const QString& right ) {
    // QString is not a guaranteed null terminal string, at least I don't find
    // any clues on its documentation, so I cannot use traditional method to
    // optimize the comparison procedure.
    int l = 0;
    int r = 0;
    const int l_len = left.size();
    const int r_len = right.size();

    while( l < l_len && r < r_len && left[l] == right[r] ) {
        ++l;
        ++r;
    }
    if( l == l_len ) {
        return false;
    } else {
        if( r == r_len ) 
            return true;
        else {
            int lcha = left[l].toLatin1();
            int rcha = right[r].toLatin1();
            if( isValidComparableAlpha(lcha) && isValidComparableAlpha(rcha) )
                return (kLookUpTable[LOOK_UP_TABLE_INDEX(lcha)] - kLookUpTable[LOOK_UP_TABLE_INDEX(rcha)]) > 0 ? true:false;
            else 
                return left[l] > right[r];
        }
    }
}

#undef LOOK_UP_TABLE_INDEX

// ----------------------------------------------------------------------
// ip V4 less than implementation. The point is that, the string representation
// of 128.1.1.1 indeed should be 128.001.001.001 and hence cannot be sorted
// simply by string. 
// -----------------------------------------------------------------------
int getNextIpV4Component( const QString& ipAddress , int index , int* nextIndex ) {
    Q_ASSERT(index < ipAddress.size());
    int idx = ipAddress.indexOf(QLatin1Char('.'),index);
    int len ;
    if( idx == -1 ) {
        // last component
        len = ipAddress.size() - index;
        *nextIndex = 0;
    } else  {
        len = idx - index;
        *nextIndex = idx + 1;
    }
    QString component = ipAddress.mid(index,len);
    bool ok;
    int ret = component.toInt(&ok);
    Q_ASSERT(ok);
    return ret;
}

bool ipV4AddressLessThan( const QString& left , const QString& right ) {
    int leftNextIndex = 0;
    int rightNextIndex =0;
    for( int i = 0 ; i < 4 ; ++i ) {
        int leftComponent = getNextIpV4Component(left,leftNextIndex,&leftNextIndex);
        int rightComponent= getNextIpV4Component(right,rightNextIndex,&rightNextIndex);
        if( leftComponent != rightComponent ) 
            return leftComponent < rightComponent;
    }
    return false;
}

}// namespace

bool QnResourceSearchProxyModel::lessThan(const QModelIndex &left, const QModelIndex &right) const {
    QVariant leftData = data(left,Qt::DisplayRole);
    QVariant rightData = data(right,Qt::DisplayRole);
    if( leftData.type() == QVariant::String && rightData.type() == QVariant::String ) {
        QString ls = leftData.toString();
        QString rs = rightData.toString();
        if( isIpv4Address(ls) ) {
            // This only happened when the target doesn't comes with
            // an ip address, it is typically an empty string here
            if(!isIpv4Address(rs))
                return false;
            else 
                return ipV4AddressLessThan(ls,rs);
        } else if( isIpv4Address(rs) ) {
            if(!isIpv4Address(ls))
                return true;
            else
                return ipV4AddressLessThan(ls,rs);
        } else
            return stringLessThan(ls,rs);
    } else {
        // Throw the rest situation to base class to handle 
        return QSortFilterProxyModel::lessThan(left,right);
    }
}
