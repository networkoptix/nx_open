#ifndef QN_EVALUATOR_H
#define QN_EVALUATOR_H

#include <QtCore/QString>
#include <QtCore/QCoreApplication>

#include "exception.h"

namespace QnExp {
    enum TokenType {
        Variable,
        Number,
        Plus,
        Minus,
        Times,
        Divide,
        LParen,
        RParen,
        Dot,
        Comma,
        End,

        Invalid = -1
    };

    enum InstructionType {
        Stor,
        Add,
        Sub,
        Mul,
        Div,
        Neg,
        Udd,
        Call,
        MCall,
        Nop = -1
    };

    QString serialized(TokenType type) {
        switch (type) {
        case Variable:  return lit("VARIABLE");
        case Number:    return lit("NUMBER");
        case Plus:      return lit("PLUS");
        case Minus:     return lit("MINUS");
        case Times:     return lit("TIMES");
        case Divide:    return lit("DIVIDE");
        case LParen:    return lit("LPAREN");
        case RParen:    return lit("RPAREN");
        case Dot:       return lit("DOT");
        case Comma:     return lit("COMMA");
        case End:       return lit("END");
        case Invalid:   return lit("INVALID");
        default:        assert(false); return QString();
        }
    }

    void serialize(TokenType type, QString *target) {
        *target = serialized(type);
    }

    QString serialized(InstructionType type) {
        switch (type) {
        case Stor:      return lit("STOR");
        case Add:       return lit("ADD");
        case Sub:       return lit("SUB");
        case Mul:       return lit("MUL");
        case Div:       return lit("DIV");
        case Neg:       return lit("NEG");
        case Udd:       return lit("UDD");
        case Call:      return lit("CALL");
        case MCall:     return lit("MCALL");
        case Nop:       return lit("NOP");
        default:        assert(false); return QString();
        }
    }

    void serialize(InstructionType type, QString *target) {
        *target = serialized(type);
    }


    class Token {
    public:
        Token(): m_type(Invalid), m_pos(-1) {}
        Token(TokenType type, const QStringRef &text, int pos): m_type(type), m_text(text), m_pos(pos) {}
        
        TokenType type() const { return m_type; }
        QStringRef text() const { return m_text; }
        int pos() const { return m_pos; }

    private:
        TokenType m_type;
        QStringRef m_text;
        int m_pos;
    };


    class Instruction {
    public:
        explicit Instruction(InstructionType type = Nop, QVariant data = QVariant()): m_type(type), m_data(data) {}

        InstructionType type() const { return m_type; }
        void setType(InstructionType type) { m_type = type; }

        const QVariant &data() const { return m_data; }
        void setData(const QVariant &data) { m_data = data; }

    private:
        InstructionType m_type;
        QVariant m_data;
    };


    class Lexer {
        Q_DECLARE_TR_FUNCTIONS(Lexer)
    public:
        Lexer(const QString &source): m_source(source + QChar(L'\0')), m_pos(0) {}

        Token readNextToken() {
            if(m_token.type() != Invalid) {
                Token result = m_token;
                m_token = Token();
                return result;
            }

            while(true) {
                QChar c = m_source[m_pos];
                switch(c.unicode()) {
                case L' ': case L'\t': case L'\n': case L'\r':
                    break;
                case L'_': 
                    return readVariableToken();
                case L'0': case L'1': case L'2': case L'3': case L'4': case L'5': case L'6': case L'7': case L'8': case L'9':
                    return readNumberToken();
                case L'-':  
                    return readSymbolToken(Minus);
                case L'+':  
                    return readSymbolToken(Plus);
                case L'*':  
                    return readSymbolToken(Times);
                case L'/':  
                    return readSymbolToken(Divide);
                case L'(':  
                    return readSymbolToken(LParen);
                case L')':  
                    return readSymbolToken(RParen);
                case L'.':
                    return readSymbolToken(Dot);
                case L',':
                    return readSymbolToken(Comma);
                case L'\0': 
                    return readSymbolToken(End);
                default:    
                    if(c.isLetter()) {
                        return readVariableToken();
                    } else {
                        unexpected();
                    }
                }
            }
        }

        Token peekNextToken() {
            if(m_token.type() == Invalid)
                m_token = readNextToken();
            return m_token;
        }

    private:
        void unexpected() const {
            throw QnException(tr("Unexpected symbol '%1' at position %2.").arg(m_source[m_pos]).arg(m_pos));
        }

        Token readNumberToken() {
            int startPos = m_pos;
            while(true) {
                switch(m_source[m_pos].unicode()) {
                case L'0': case L'1': case L'2': case L'3': case L'4': case L'5': case L'6': case L'7': case L'8': case L'9':
                    m_pos++;
                    break;
                default:
                    return Token(Number, m_source.midRef(startPos, m_pos - startPos), startPos);
                }
            }
        }

        Token readVariableToken() {
            int startPos = m_pos;
            while(true) {
                QChar c = m_source[m_pos];
                if(c.isLetterOrNumber() || c.unicode() == L'_') {
                    m_pos++;
                    continue;
                } else {
                    return Token(Variable, m_source.midRef(startPos, m_pos - startPos), startPos);
                }
            }
        }

        Token readSymbolToken(TokenType type) {
            m_pos++;
            return Token(type, m_source.midRef(m_pos - 1, 1), m_pos - 1);
        }

    private:
        QString m_source;
        int m_pos;
        Token m_token;
    };


    class Parser {
        Q_DECLARE_TR_FUNCTIONS(Parser)
    public:
        Parser(Lexer *lexer): m_lexer(lexer) {};

        QVector<Instruction> parse() {
            m_rpn.clear();
            parseExpr();
            
            Token token = m_lexer->peekNextToken();
            if(token.type() != End)
                unexpected(token);

            return m_rpn;
        }

    private:
        void unexpected(const Token &token) const {
            throw QnException(tr("Unexpected token %1 ('%2') at position %3.").arg(serialized(token.type())).arg(token.text().toString()).arg(token.pos()));
        }

        void require(TokenType type) {
            Token token = m_lexer->readNextToken();
            if(token.type() != type)
                unexpected(token);
        }

        void parseArgs() {
            /* args ::= '('')' | '(' expr {',' expr} ')' | */
            Token token = m_lexer->peekNextToken();
            if(token.type() != LParen) {
                m_rpn.push_back(Instruction(Call, 0));
                return;
            }

            require(LParen);

            token = m_lexer->peekNextToken();
            if(token.type() == RParen) {
                require(RParen);
                m_rpn.push_back(Instruction(Call, 0));
                return;
            }
            
            int argc = 1;
            parseExpr();

            while(true) {
                token = m_lexer->peekNextToken();
                if(token.type() == Comma) {
                    require(Comma);
                    parseExpr();
                    argc++;
                } else {
                    m_rpn.push_back(Instruction(Call, argc));
                    return;
                }
            }
        }

        void parseInvocation() {
            /* invocation ::= VAR args */
            Token token = m_lexer->peekNextToken();
            if(token.type() == Variable) {
                require(Variable);
                m_rpn.push_back(Instruction(Stor, token.text().toString()));
            } else {
                unexpected(token);
            }

            parseArgs();
        }

        void parseChain() {
            /* chain ::= invocation {'.' invocation} */
            Token token = m_lexer->peekNextToken();
            if(token.type() == Variable) {
                parseInvocation();
            } else {
                unexpected(token);
            }

            while(true) {
                token = m_lexer->peekNextToken();
                if(token.type() == Dot) {
                    require(Dot);
                    parseInvocation();

                    /* Replace Call with MCall */
                    m_rpn.back().setType(MCall);
                } else {
                    return;
                }
            }
        }

        void parseFactor() {
            /* factor ::= chain | INT | '(' expr ')' | ('-' | '+') factor */
            Token token = m_lexer->peekNextToken();
            switch(token.type()) {
            case Number:
                require(Number);
                m_rpn.push_back(Instruction(Stor, token.text().toInt())); // TODO: throw on int parse error
                break;

            case Variable:
                parseChain();
                break;

            case LParen:
                require(LParen);
                parseExpr();
                require(RParen);
                break;
            
            case Minus:
            case Plus:
                require(token.type());
                parseFactor();
                m_rpn.push_back(Instruction(token.type() == Plus ? Udd : Neg));
                break;

            default:
                unexpected(token);
            }
        }

        void parseTerm() {
            /* term ::= factor {('*' | '/') factor} */
            Token token = m_lexer->peekNextToken();
            switch(token.type()) {
            case Number:
            case LParen:
            case Minus:
            case Plus:
                parseFactor();
                break;

            default:
                unexpected(token);
            }

            while(true) {
                token = m_lexer->peekNextToken();
                switch(token.type()) {
                case Times:
                case Divide:
                    require(token.type());
                    parseFactor();
                    m_rpn.push_back(Instruction(token.type() == Times ? Mul : Div));
                    break;
                default:
                    return;                
                }
            }
        }

        void parseExpr() {
            /* expr ::= term {('+' | '-') term} */
            Token token = m_lexer->peekNextToken();
            switch(token.type()) {
            case Number:
            case LParen:
            case Minus:
            case Plus:
                parseTerm();
                break;

            default:
                unexpected(token);
            }

            while(true) {
                token = m_lexer->peekNextToken();
                switch(token.type()) {
                case Plus:
                case Minus:
                    require(token.type());
                    parseTerm();
                    m_rpn.push_back(Instruction(token.type() == Plus ? Add : Sub));
                    break;
                default:
                    return;
                }
            }
        }

    private:
        Lexer *m_lexer;
        QVector<Instruction> m_rpn; /* Reverse Polish Notation */
    };


    class Calculator {
        Q_DECLARE_TR_FUNCTIONS(Calculator)
    public:
        Calculator(Parser *parser): m_parser(parser) {}

        double calculate() {
            QVector<Instruction> rpn = m_parser->parse();
            QVector<double> stack;

            for(unsigned int i = 0; i < rpn.size(); i++) {
                Instruction instruction = rpn[i];
                double v1, v2;
                switch(instruction.type()) {
                case Stor:
                    stack.push_back(instruction.text().toDouble());
                    break;
                case Add:
                    v2 = pop_back(stack);
                    v1 = pop_back(stack);
                    stack.push_back(v1 + v2);
                    break;
                case Sub:
                    v2 = pop_back(stack);
                    v1 = pop_back(stack);
                    stack.push_back(v1 - v2);
                    break;
                case Mul:
                    v2 = pop_back(stack);
                    v1 = pop_back(stack);
                    stack.push_back(v1 * v2);
                    break;
                case Div:
                    v2 = pop_back(stack);
                    v1 = pop_back(stack);
                    stack.push_back(v1 / v2);
                    break;
                case Neg:
                    v1 = pop_back(stack);
                    stack.push_back(- v1);
                    break;
                case Udd:
                    v1 = pop_back(stack);
                    stack.push_back(+ v1);
                    break;
                default:
                    assert(false);
                }
            }

            if(stack.size() != 1)
                throw QnException(tr("Invalid stack size after RPN evaluation: %1.").arg(rpn.size()));

            return stack[0];
        }

    private:
        void unexpected(const Token &token) const {
            throw QnException(tr("Unexpected token %1 (%2) in RPN at position %3.").arg(serialized(token.type())).arg(token.text().toString()).arg(token.pos()));
        }

        double pop_back(QVector<double> &stack) {
            if(stack.isEmpty())
                throw QnException(tr("Invalid stack size during RPN evaluation: 0."));
            
            double result = stack.back();
            stack.pop_back();
            return result;
        }

    private:
        Parser *m_parser;
    };

} // namespace QnExp

#endif // QN_EVALUATOR_H
