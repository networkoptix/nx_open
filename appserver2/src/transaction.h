
#ifndef EC2_TRANSACTION_H
#define EC2_TRANSACTION_H

#include <vector>


class Variant
{
public:
    enum Type
    {
        //TODO
    };

    union
    {
        qint32 i32;
        qint64 i64;
        double d;
        QString* str;
    };
};

namespace ec2
{
    enum class ActionType
    {
        add,
        update,
        remove
    };

    class QnTransaction
    {
    public:
        struct ID
        {
            GUID serverGUID;
            qint64 number;
        };

        QnTransaction( const QnTransaction&& xvalue );

        ID id;

        bool persistent;
        ActionType action;
        EntityType entityType;
        //todo: static field list or dynamic?
        std::vector<Variant> actionParams;
    };
}

QN_DEFINE_STRUCT_BINARY_SERIALIZATION_FUNCTIONS( QnTransaction, ... )

#endif  //EC2_TRANSACTION_H
