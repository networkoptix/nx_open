#pragma once

#include <QtCore/QHash> 

namespace std {

template<> struct hash<QString>
{
    std::size_t operator()(const QString& s) const
    {
        return qHash(s);
    }
};

} // namespace std
