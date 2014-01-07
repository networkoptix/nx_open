#ifndef QN_EVALUATOR_H
#define QN_EVALUATOR_H

#include <QtCore/QString>
#include <QtCore/QCoreApplication>

#include "exception.h"

namespace QnExp {
    enum TokenType {
        Number,
        Plus,
        Minus,
        Times,
        Divide,
        Lparen,
        Rparen,

        Uminus,
        Uplus,
        End,

        Invalid
    };

    QString serialized(TokenType type) {
        switch (type) {
        case Number:    return lit("NUMBER");
        case Plus:      return lit("PLUS");
        case Minus:     return lit("MINUS");
        case Times:     return lit("TIMES");
        case Divide:    return lit("DIVIDE");
        case Lparen:    return lit("LPAREN");
        case Rparen:    return lit("RPAREN");
        case Uminus:    return lit("UMINUS");
        case Uplus:     return lit("UPLUS");
        case End:       return lit("END");
        case Invalid:   return lit("INVALID");
        default:        assert(false);
        }
    }

    void serialize(TokenType type, QString *target) {
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
                switch(m_source[m_pos].unicode()) {
                case L' ': case L'\t': case L'\n': case L'\r':
                    break;
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
                    return readSymbolToken(Lparen);
                case L')':  
                    return readSymbolToken(Rparen);
                case L'\0': 
                    return readSymbolToken(End);
                default:    
                    unexpected();
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

        QVector<Token> parse() {
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

        void parseFactor() {
            /* factor ::= INT | '(' expr ')' | ('-' | '+') factor */
            Token token = m_lexer->peekNextToken();
            switch(token.type()) {
            case Number:
                require(Number);
                m_rpn.push_back(token);
                break;
            
            case Lparen:
                require(Lparen);
                parseExpr();
                require(Rparen);
                break;
            
            case Minus:
            case Plus:
                require(token.type());
                parseFactor();
                if(token.type() == Plus) {
                    m_rpn.push_back(Token(Uplus, token.text(), token.pos()));
                } else if(token.type() == Minus) {
                    m_rpn.push_back(Token(Uminus, token.text(), token.pos()));
                }
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
            case Lparen:
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
                    m_rpn.push_back(token);
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
            case Lparen:
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
                    m_rpn.push_back(token);
                    break;
                default:
                    return;
                }
            }
        }

    private:
        Lexer *m_lexer;
        QVector<Token> m_rpn; /* Reverse Polish Notation */
    };


    class Calculator {
        Q_DECLARE_TR_FUNCTIONS(Calculator)
    public:
        Calculator(Parser *parser): m_parser(parser) {}

        double calculate() {
            QVector<Token> rpn = m_parser->parse();
            QVector<double> stack;

            for(unsigned int i = 0; i < rpn.size(); i++) {
                Token token = rpn[i];
                double v1, v2;
                switch(token.type()) {
                case Number:
                    stack.push_back(token.text().toDouble());
                    break;
                case Plus:
                    v2 = pop_back(stack);
                    v1 = pop_back(stack);
                    stack.push_back(v1 + v2);
                    break;
                case Minus:
                    v2 = pop_back(stack);
                    v1 = pop_back(stack);
                    stack.push_back(v1 - v2);
                    break;
                case Times:
                    v2 = pop_back(stack);
                    v1 = pop_back(stack);
                    stack.push_back(v1 * v2);
                    break;
                case Divide:
                    v2 = pop_back(stack);
                    v1 = pop_back(stack);
                    stack.push_back(v1 / v2);
                    break;
                case Uminus:
                    v1 = pop_back(stack);
                    stack.push_back(- v1);
                    break;
                case Uplus:
                    v1 = pop_back(stack);
                    stack.push_back(+ v1);
                    break;
                default:
                    unexpected(token);
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
