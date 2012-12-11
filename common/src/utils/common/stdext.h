
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
        return binder<Operation, Param>( _op, _param );
    }

    template<class Operation, class Param1, class Param2, class Param3, class Param4, class ResultType>
    struct binder4
    {
        binder4( const Operation& _op, const Param1& _param1, const Param2& _param2, const Param3& _param3, const Param4& _param4 )
        :
            op( _op ),
            param1( _param1 ),
            param2( _param2 ),
            param3( _param3 ),
            param4( _param4 )
        {
        }

        ResultType operator()() const
        {
            return op( param1, param2, param3, param4 );
        }

    private:
        Operation op;
        Param1 param1;
        Param2 param2;
        Param3 param3;
        Param4 param4;
    };

    template<class Param1, class Param2, class Param3, class Param4, class ResultType>
        binder4<ResultType (*)( Param1, Param2, Param3, Param4 ), Param1, Param2, Param3, Param4, ResultType> bind(
            ResultType (*_op)( Param1, Param2, Param3, Param4 ),
            const Param1& _param1,
            const Param2& _param2,
            const Param3& _param3,
            const Param4& _param4 )
    {
        return binder4<ResultType (*)( Param1, Param2, Param3, Param4 ), Param1, Param2, Param3, Param4, ResultType>(
            _op,
            _param1,
            _param2,
            _param3,
            _param4 );
    }

    template<class Operation, class Param1, class Param2, class Param3, class Param4, class Param5, class Param6, class ResultType>
    struct binder6
    {
        binder6( const Operation& _op, const Param1& _param1, const Param2& _param2, const Param3& _param3, const Param4& _param4, const Param5& _param5, const Param6& _param6 )
        :
            op( _op ),
            param1( _param1 ),
            param2( _param2 ),
            param3( _param3 ),
            param4( _param4 ),
            param5( _param5 ),
            param6( _param6 )
        {
        }

        ResultType operator()() const
        {
            return op( param1, param2, param3, param4, param5, param6 );
        }

    private:
        Operation op;
        Param1 param1;
        Param2 param2;
        Param3 param3;
        Param4 param4;
        Param5 param5;
        Param6 param6;
    };

    template<class Param1, class Param2, class Param3, class Param4, class Param5, class Param6, class ResultType>
        binder6<ResultType (*)( Param1, Param2, Param3, Param4, Param5, Param6 ), Param1, Param2, Param3, Param4, Param5, Param6, ResultType> bind(
            ResultType (*_op)( Param1, Param2, Param3, Param4, Param5, Param6 ),
            const Param1& _param1,
            const Param2& _param2,
            const Param3& _param3,
            const Param4& _param4,
            const Param5& _param5,
            const Param6& _param6 )
    {
        return binder6<ResultType (*)( Param1, Param2, Param3, Param4, Param5, Param6 ), Param1, Param2, Param3, Param4, Param5, Param6, ResultType>(
            _op,
            _param1,
            _param2,
            _param3,
            _param4,
            _param5,
            _param6 );
    }
}

#endif  //NX_STDEXT_H
