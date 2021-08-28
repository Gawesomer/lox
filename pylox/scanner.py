from lox_token import Token
from token_type import TokenType


class Scanner:

    keywords = {
        "and": TokenType.AND,
        "break": TokenType.BREAK,
        "class": TokenType.CLASS,
        "else": TokenType.ELSE,
        "false": TokenType.FALSE,
        "for": TokenType.FOR,
        "fun": TokenType.FUN,
        "if": TokenType.IF,
        "import": TokenType.IMPORT,
        "nil": TokenType.NIL,
        "or": TokenType.OR,
        "print": TokenType.PRINT,
        "return": TokenType.RETURN,
        "this": TokenType.THIS,
        "true": TokenType.TRUE,
        "var": TokenType.VAR,
        "while": TokenType.WHILE,
    }

    def __init__(self, reporter: "Lox", source: str):
        self.reporter = reporter
        self.source = source
        self.tokens = []

        self.start = 0
        self.current = 0
        self.line = 1

    def scan_tokens(self) -> list[Token]:
        while not self.is_at_end():
            # We are at the beginning of the next lexeme.
            self.start = self.current
            scanned_token = self.scan_token()
            if scanned_token is not None:
                self.tokens.append(scanned_token)

        self.tokens.append(Token(TokenType.EOF, "", None, self.line))
        return self.tokens

    def scan_token(self) -> Token:
        c = self.advance()
        if c == '(':
            return self.new_token(TokenType.LEFT_PAREN)
        elif c == ')':
            return self.new_token(TokenType.RIGHT_PAREN)
        elif c == '{':
            return self.new_token(TokenType.LEFT_BRACE)
        elif c == '}':
            return self.new_token(TokenType.RIGHT_BRACE)
        elif c == '[':
            return self.new_token(TokenType.LEFT_BRACKET)
        elif c == ']':
            return self.new_token(TokenType.RIGHT_BRACKET)
        elif c == ',':
            return self.new_token(TokenType.COMMA)
        elif c == '.':
            return self.new_token(TokenType.DOT)
        elif c == '-':
            return self.new_token(TokenType.MINUS)
        elif c == '+':
            return self.new_token(TokenType.PLUS)
        elif c == ';':
            return self.new_token(TokenType.SEMICOLON)
        elif c == '*':
            return self.new_token(TokenType.STAR)
        elif c == '?':
            return self.new_token(TokenType.EROTEME)
        elif c == ':':
            return self.new_token(TokenType.COLON)
        elif c == '!':
            return self.new_token(TokenType.BANG_EQUAL if self.match('=') else TokenType.BANG)
        elif c == '=':
            return self.new_token(TokenType.EQUAL_EQUAL if self.match('=') else TokenType.EQUAL)
        elif c == '<':
            return self.new_token(TokenType.LESS_EQUAL if self.match('=') else TokenType.LESS)
        elif c == '>':
            return self.new_token(TokenType.GREATER_EQUAL if self.match('=') else TokenType.GREATER)
        elif c == '/':
            return self.comment()
        elif c in (' ', '\r', '\t'):
            # Ignore whitespace.
            return None
        elif c == '\n':
            self.line += 1
            return None
        elif c == '"':
            return self.string()
        elif c.isdigit():
            return self.number()
        elif c.isalpha():
            return self.identifier()
        else:
            self.reporter.error(self.line, "Unexpected character.")
            return None

    def is_at_end(self) -> bool:
        return self.current >= len(self.source)

    def advance(self) -> str:
        self.current += 1
        return self.source[self.current-1]

    def new_token(self, token_type: TokenType, literal: object = None) -> Token:
        text = self.source[self.start:self.current]
        return Token(token_type, text, literal, self.line)

    def match(self, expected: str) -> bool:
        if self.is_at_end() or self.source[self.current] != expected:
            return False

        self.current += 1
        return True

    def peek(self) -> str:
        if self.is_at_end():
            return '\0'
        return self.source[self.current]

    def peek_next(self) -> str:
        if self.current+1 >= len(self.source):
            return '\0'
        return self.source[self.current+1]

    def string(self) -> Token:
        while self.peek() != '"' and not self.is_at_end():
            if self.peek() == '\n':
                self.line += 1
            self.advance()

        if self.is_at_end():
            self.reporter.error(self.line, "Unterminated string.")
            return None

        # The closing ".
        self.advance()

        # Trim the surrounding quotes.
        value = self.source[self.start+1:self.current-1]
        return self.new_token(TokenType.STRING, value)

    def number(self) -> Token:
        while self.peek().isdigit():
            self.advance()

        # Look for a fractional part.
        if self.peek() == '.' and self.peek_next().isdigit():
            # Consume the "."
            self.advance()

            while self.peek().isdigit():
                self.advance()

        return self.new_token(TokenType.NUMBER, float(self.source[self.start:self.current]))

    def identifier(self) -> Token:
        while self.peek().isalnum():
            self.advance()

        text = self.source[self.start:self.current]
        token_type = self.keywords.get(text, TokenType.IDENTIFIER)

        if token_type == TokenType.IMPORT:
            while self.peek() in (' ', '\r', '\t'):
                self.advance()
            self.start = self.current
            while self.peek() != ';' and not self.is_at_end():
                self.advance()

        return self.new_token(token_type)

    def comment(self) -> Token:
        if self.match('/'):     # Line comment.
            # A comment goes until the end of the line.
            while self.peek() != '\n' and not self.is_at_end():
                self.advance()
            return None
        elif self.match('*'):   # Block comment.
            while not (self.peek() == '*' and self.peek_next() == '/') and not self.is_at_end():
                if self.peek() == '\n':
                    self.line += 1
                self.advance()

            if self.is_at_end():
                self.reporter.error(self.line, "Unterminated block comment.")
                return None

            # Consume the "*/"
            self.advance()
            self.advance()
            return None
        else:
            return self.new_token(TokenType.SLASH)
