
#ifndef SERIALIZATION_HELPER_H
#define SERIALIZATION_HELPER_H

#include <boost/preprocessor/seq/for_each.hpp>


 struct Transaction {
     int fld1;
     int fld2;
     int fld3;
     QList<int> fld4;
 };

 (fld1)(fld2)

#define QN_DEFINE_STRUCT_BINARY_SERIALIZATION_FUNCTIONS(TYPE, FIELD_SEQ, ... /* PREFIX */) \
    template<class T>                                                   \
    __VA_ARGS__ void serialize(const TYPE &value, BinaryStream<T> *target) { \
       BOOST_PP_SEQ_FOR_EACH(FIELD_SEQ, MACRO, ~) \
    } \
    \
    __VA_ARGS__ bool deserialize()

    MACRO(0, ~, fld1)
    MACRO(0, ~, fld2)


#define foreach(SEQ)
    evaluate ## SEQ _end

#define evaluate(elem)
    MACRO(elem) evaluate2 ##

#define evaluate2(elem)
    MACRO(elem) evaluate3 ##

...

#define evaluate_end
#define evaluate2_end
#define evaluate3_end



#define MACRO(R, D, FIELD) \
    QnBinary::serialize(value.FIELD, target);


template<class T>
class BinaryStream; /* asdas */

template<>
class BinaryStream<QIODevice> {
};

template<>
class BinaryStream<QByteArray> {
};


template<class T>
void serialize( Transaction, BinaryStream<T> );

template<class T>
void deserialize( BinaryStream<T> , Transaction * );

void serialize( Transaction, QJsonValue * );
void deserialize( const QJsonValue &, Transaction * );


 /*serialize( const Transaction &, BinaryStream * );

 serialize( const Transaction &, QJsonValue * );*/

#endif  //SERIALIZATION_HELPER_H
