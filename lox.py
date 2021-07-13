import sys

import expr
from ast_printer import ASTPrinter
from token import Token
from token_type import TokenType


class Lox:

    had_error = False

    @classmethod
    def main(cls):
        if len(sys.argv) > 2:
          print("Usage: jlox [script]")
          exit(1)
        elif len(sys.argv) == 2:
          cls.run_file(sys.argv[1])
        else:
          cls.run_prompt()


    @classmethod
    def run_file(cls, filename: str):
        with open(filename, 'r') as f:
            code = f.read()
            cls.run(code)
        if cls.had_error:
            exit(1)


    @classmethod
    def run_prompt(cls):
        while True:
            try:
                line = input("> ")
                cls.run(line)
                cls.had_error = False
            except EOFError:
                break


    @classmethod
    def run(cls, source: str):
        scanner = Scanner(source)
        tokens = scanner.scan_tokens()
        """
        for token in tokens:
            print(token)
        """
        parser = Parser(tokens)
        expression = parser.parse()

        # Stop if there was a syntax error.
        if cls.had_error:
            return

        print(ASTPrinter().print(expression))


    @classmethod
    def error(cls, line: int, message: str):
        cls.report(line, "", message)


    @classmethod
    def report(cls, line: int, where: str, message: str):
        print("[line {}] Error{}: {}".format(line, where, message), file=sys.stderr)
        cls.had_error = True


    @classmethod
    def error_parser(cls, token: Token, message: str):
        if token.type == TokenType.EOF:
            cls.report(token.line, " at end", message)
        else:
            cls.report(token.line, " at '{}'".format(token.lexeme), message)


class Scanner:

    keywords = {
        "and": TokenType.AND,
        "class": TokenType.CLASS,
        "else": TokenType.ELSE,
        "false": TokenType.FALSE,
        "for": TokenType.FOR,
        "fun": TokenType.FUN,
        "if": TokenType.IF,
        "nil": TokenType.NIL,
        "or": TokenType.OR,
        "print": TokenType.PRINT,
        "return": TokenType.RETURN,
        "super": TokenType.SUPER,
        "this": TokenType.THIS,
        "true": TokenType.TRUE,
        "var": TokenType.VAR,
        "while": TokenType.WHILE,
    }

    def __init__(self, source: str):
        self.source = source
        self.tokens = []

        self.start = 0
        self.current = 0
        self.line = 1


    def scan_tokens(self):
        while not self.is_at_end():
            # We are at the beginning of the next lexeme.
            self.start = self.current
            self.scan_token()

        self.tokens.append(Token(TokenType.EOF, "", None, self.line))
        return self.tokens


    def scan_token(self):
        c = self.advance()
        if c == '(':
            self.add_token(TokenType.LEFT_PAREN)
        elif c == ')':
            self.add_token(TokenType.RIGHT_PAREN)
        elif c == '{':
            self.add_token(TokenType.LEFT_BRACE)
        elif c == '}':
            self.add_token(TokenType.RIGHT_BRACE)
        elif c == ',':
            self.add_token(TokenType.COMMA)
        elif c == '.':
            self.add_token(TokenType.DOT)
        elif c == '-':
            self.add_token(TokenType.MINUS)
        elif c == '+':
            self.add_token(TokenType.PLUS)
        elif c == ';':
            self.add_token(TokenType.SEMICOLON)
        elif c == '*':
            self.add_token(TokenType.STAR)
        elif c == '!':
            self.add_token(TokenType.BANG_EQUAL if self.match('=') else TokenType.BANG)
        elif c == '=':
            self.add_token(TokenType.EQUAL_EQUAL if self.match('=') else TokenType.EQUAL)
        elif c == '<':
            self.add_token(TokenType.LESS_EQUAL if self.match('=') else TokenType.LESS)
        elif c == '>':
            self.add_token(TokenType.GREATER_EQUAL if self.match('=') else TokenType.GREATER)
        elif c == '/':
            self.comment()
        elif c in (' ', '\r', '\t'):
            # Ignore whitespace.
            pass
        elif c == '\n':
            self.line += 1
        elif c == '"':
            self.string()
        elif c.isdigit():
            self.number()
        elif c.isalpha():
            self.identifier()
        else:
            Lox.error(self.line, "Unexpected character.")


    def is_at_end(self):
        return self.current >= len(self.source)


    def advance(self) -> str:
        self.current += 1
        return self.source[self.current-1]


    def add_token(self, token_type: TokenType, literal=None):
        text = self.source[self.start:self.current]
        self.tokens.append(Token(token_type, text, literal, self.line))


    def match(self, expected: str) -> bool:
        if self.is_at_end() or self.source[self.current] != expected:
            return False

        self.current += 1
        return True


    def peek(self) -> str:
        if self.is_at_end():
            return '\0'
        return self.source[self.current]


    def peek_next(self):
        if self.current+1 >= len(self.source):
            return '\0'
        return self.source[self.current+1]


    def string(self):
        while self.peek() != '"' and not self.is_at_end():
            if self.peek() == '\n':
                self.line += 1
            self.advance()

        if self.is_at_end():
            Lox.error(self.line, "Unterminated string.")
            return

        # The closing ".
        self.advance()

        # Trim the surrounding quotes.
        value = self.source[self.start+1:self.current-1]
        self.add_token(TokenType.STRING, value)


    def number(self):
        while self.peek().isdigit():
            self.advance()

        # Look for a fractional part.
        if self.peek() == '.' and self.peek_next().isdigit():
            # Consume the "."
            self.advance()

            while self.peek().isdigit():
                self.advance()

        self.add_token(TokenType.NUMBER, float(self.source[self.start:self.current]))


    def identifier(self):
        while self.peek().isalnum():
            self.advance()

        text = self.source[self.start:self.current]
        token_type = self.keywords.get(text, TokenType.IDENTIFIER)

        self.add_token(token_type)


    def comment(self):
        if self.match('/'):     # Line comment.
            # A comment goes until the end of the line.
            while self.peek() != '\n' and not self.is_at_end():
                self.advance()
        elif self.match('*'):   # Block comment.
            while not (self.peek() == '*' and self.peek_next() == '/') and not self.is_at_end():
                if self.peek() == '\n':
                    self.line += 1
                self.advance()

            if self.is_at_end():
                Lox.error(self.line, "Unterminated block comment.")
                return

            # Consume the "*/"
            self.advance()
            self.advance()
        else:
            self.add_token(TokenType.SLASH)


class Parser:
    """
    Expression grammar:
        expression     → equality ;
        equality       → comparison ( ( "!=" | "==" ) comparison )* ;
        comparison     → term ( ( ">" | ">=" | "<" | "<=" ) term )* ;
        term           → factor ( ( "-" | "+" ) factor )* ;
        factor         → unary ( ( "/" | "*" ) unary )* ;
        unary          → ( "!" | "-" ) unary
                       | primary ;
        primary        → NUMBER | STRING | "true" | "false" | "nil"
                       | "(" expression ")" ;
    """

    class ParseError(Exception):
        pass

    def __init__(self, tokens: list[Token]):
        self.tokens = tokens
        self.current = 0


    def parse(self) -> expr.Expr:
        try:
            return self.expression()
        except self.ParseError:
            return None


    def expression(self) -> expr.Expr:
        return self.equality()


    def equality(self) -> expr.Expr:
        expression = self.comparison()

        while self.match(TokenType.BANG_EQUAL, TokenType.EQUAL_EQUAL):
            operator = self.previous()
            right = self.comparison()
            expression = expr.Binary(expression, operator, right)

        return expression


    def comparison(self) -> expr.Expr:
        expression = self.term()

        while self.match(TokenType.GREATER, TokenType.GREATER_EQUAL, TokenType.LESS, TokenType.LESS_EQUAL):
            operator = self.previous()
            right = self.term()
            expression = expr.Binary(expression, operator, right)

        return expression


    def term(self) -> expr.Expr:
        expression = self.factor()

        while self.match(TokenType.MINUS, TokenType.PLUS):
            operator = self.previous()
            right = self.factor()
            expression = expr.Binary(expression, operator, right)

        return expression


    def factor(self) -> expr.Expr:
        expression = self.unary()

        while self.match(TokenType.SLASH, TokenType.STAR):
            operator = self.previous()
            right = self.factor()
            expression = expr.Binary(expression, operator, right)

        return expression


    def unary(self) -> expr.Expr:
        if self.match(TokenType.BANG, TokenType.MINUS):
            operator = self.previous()
            right = self.unary()
            return expr.Unary(operator, right)

        return self.primary()


    def primary(self) -> expr.Expr:
        if self.match(TokenType.FALSE):
            return expr.Literal(False)
        if self.match(TokenType.TRUE):
            return expr.Literal(True)
        if self.match(TokenType.NIL):
            return expr.Literal(None)

        if self.match(TokenType.NUMBER, TokenType.STRING):
            return expr.Literal(self.previous().literal)

        if self.match(TokenType.LEFT_PAREN):
            expression = self.expression()
            self.consume(TokenType.RIGHT_PAREN, "Expect ')' after expression.")
            return expr.Grouping(expression)

        raise self.error(self.peek(), "Expect expression.")


    def match(self, *types) -> bool:
        for type_ in types:
            if self.check(type_):
                self.advance()
                return True

        return False


    def consume(self, token_type: TokenType, message: str) -> Token:
        if self.check(token_type):
            return self.advance()

        raise self.error(self.peek(), message)


    def check(self, token_type: TokenType) -> bool:
        if self.is_at_end():
            return False
        return self.peek().type == token_type


    def advance(self) -> Token:
        if not self.is_at_end():
            self.current += 1
        return self.previous()


    def is_at_end(self) -> bool:
        return self.peek().type == TokenType.EOF


    def peek(self) -> Token:
        return self.tokens[self.current]


    def previous(self) -> Token:
        return self.tokens[self.current-1]


    def error(self, token: Token, message: str) -> Exception:
        Lox.error_parser(token, message)
        return self.ParseError()


    def synchronize(self):
        self.advance()

        while not self.is_at_end():
            if self.previous().type == TokenType.SEMICOLON:
                return

            if self.peek().type in [
                TokenType.CLASS,
                TokenType.FUN,
                TokenType.VAR,
                TokenType.FOR,
                TokenType.IF,
                TokenType.WHILE,
                TokenType.PRINT,
                TokenType.RETURN]:
                return

            self.advance()


if __name__ == "__main__":
    Lox.main()
