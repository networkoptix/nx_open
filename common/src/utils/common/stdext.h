
#ifndef NX_STDEXT_H
#define NX_STDEXT_H


namespace stdext
{
    //TODO throw this shit away on move to c++11 compiler

    template<class Operation, class Param>
    struct binder
    {
        Operation op;
        Param param;

        binder( const Operation& _op, const Param& _param )
        :
            op( _op ),
            param( _param )
        {
        }

        void operator()()
        {
            op( param );
        }
    };

    template<class Operation, class Param>
        binder<Operation, Param> bind( const Operation& _op, const Param& _param )
    {
        return typename binder<Operation, Param>( _op, _param );
    }
}

#endif  //NX_STDEXT_H
