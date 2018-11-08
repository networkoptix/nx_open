
#include "buttons.h"

#include <helpers/model_change_helper.h>

namespace
{
    enum
    {
        kFirstCustomRoleId = Qt::UserRole + 1
    
        , kIdRoleId = kFirstCustomRoleId
        , kCaptionRoleId
        , kAspectRoleId
        
        , kLastCustomRoleId        
    };
    
    const rtu::Roles kRoles = []() -> rtu::Roles
    {
        rtu::Roles roles;
        roles.insert(kIdRoleId, "id");
        roles.insert(kCaptionRoleId, "caption");
        roles.insert(kAspectRoleId, "aspect");
        return roles;
    }();
    
    struct ButtonInfo
    {
        QString name;
        bool isStyled;
        float aspect;
        
        ButtonInfo();
        
        ButtonInfo(const QString &name
            , bool isStyled
            , float aspect);
    };

    ButtonInfo::ButtonInfo() {}
    
    ButtonInfo::ButtonInfo(const QString &initName
        , bool initIsStyled
        , float initAspect)
        : name(initName)
        , isStyled(initIsStyled)
        , aspect(initAspect)
    {
    }

    typedef QMap<int, ButtonInfo> AvailableButtons;
    const AvailableButtons availableButtons = []() -> AvailableButtons
    {
        enum
        {
            kShortButtonAspect = 3
            , kMediumButtonAspect = 6
        };
        
        AvailableButtons result;
        result.insert(rtu::Buttons::Yes, ButtonInfo("Yes", true, kShortButtonAspect));
        result.insert(rtu::Buttons::Ok, ButtonInfo("Ok", true, kShortButtonAspect));
        result.insert(rtu::Buttons::No, ButtonInfo("No", false, kShortButtonAspect));
        result.insert(rtu::Buttons::Cancel, ButtonInfo("Cancel", false, kShortButtonAspect));
        result.insert(rtu::Buttons::ApplyChanges, ButtonInfo("Apply changes", false, kMediumButtonAspect));
        result.insert(rtu::Buttons::DiscardChanges, ButtonInfo("Discard changes", false, kMediumButtonAspect));
        result.insert(rtu::Buttons::Update, ButtonInfo("Update", false, kShortButtonAspect));
        return result;
    }();
}

class rtu::Buttons::Impl : public QObject
{
public:
    Impl(rtu::Buttons *owner
        , rtu::ModelChangeHelper *changeHelper);
    
    virtual ~Impl();
    
    int buttons() const;
    
    void setButtons(int buttons);
    
public:
    int rowCount() const;
    
    QVariant data(const QModelIndex &index
        , int role) const;

private:
    typedef QPair<int, ButtonInfo> ButtonFullInfo;
    typedef QVector<ButtonFullInfo> ButtonInfosContainer;
    
    rtu::ModelChangeHelper * const m_changeHelper;
    int m_flags;
    ButtonInfosContainer m_buttons;
};

rtu::Buttons::Impl::Impl(rtu::Buttons *owner
    , rtu::ModelChangeHelper *changeHelper)
    : QObject(owner)
    , m_changeHelper(changeHelper)
    , m_flags(0)
    , m_buttons()
{
}

rtu::Buttons::Impl::~Impl()
{
}

int rtu::Buttons::Impl::buttons() const
{
    return m_flags;
}

void rtu::Buttons::Impl::setButtons(int buttons)
{
    m_flags = buttons;
    const auto &guard = m_changeHelper->resetModelGuard();
    Q_UNUSED(guard);
    
    m_buttons.clear();
    for (auto it = availableButtons.begin(); it != availableButtons.end(); ++it)
    {
        if (!(it.key() & buttons))
            continue;
        m_buttons.push_back(ButtonFullInfo(it.key(), it.value()));
    }
}

int rtu::Buttons::Impl::rowCount() const
{
    return m_buttons.size();
}

QVariant rtu::Buttons::Impl::data(const QModelIndex &index
    , int role) const
{
    const int row = index.row();
    if ((row < 0) || (row >= rowCount())
        || (role < kIdRoleId) || (role >= kLastCustomRoleId))
    {
        return QVariant();
    }
    
    const auto &button = m_buttons.at(row);
    switch (role) 
    {
    case kIdRoleId:
        return button.first;
    case kCaptionRoleId:
        return button.second.name;
    case kAspectRoleId:
        return button.second.aspect;
    default:
        return QVariant();
    }
}

///

rtu::Buttons::Buttons(QObject *parent)
    : QAbstractListModel(parent)
    , m_impl(new Impl(this, CREATE_MODEL_CHANGE_HELPER(this)))
{
    
}

rtu::Buttons::~Buttons()
{
}

int rtu::Buttons::buttons() const
{
    return m_impl->buttons();
}

void rtu::Buttons::setButtons(int buttons)
{
    m_impl->setButtons(buttons);
}

int rtu::Buttons::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    return m_impl->rowCount();
}

QVariant rtu::Buttons::data(const QModelIndex &index
    , int role) const
{
    return m_impl->data(index, role);
}

rtu::Roles rtu::Buttons::roleNames() const
{
    return kRoles;
}

