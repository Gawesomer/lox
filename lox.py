import sys

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
        for token in tokens:
            print(token)


    @classmethod
    def error(cls, line: int, message: str):
        cls.report(line, "", message)


    @classmethod
    def report(cls, line: int, where: str, message: str):
        print("[line {}] Error{}: {}".format(line, where, message), file=sys.stderr)
        cls.had_error = True


class Scanner:

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
            if self.match('/'):
                # A comment goes until the end of the line.
                while self.peek() != '\n' and not self.is_at_end():
                    self.advance()
            else:
                self.add_token(TokenType.SLASH)
        elif c in (' ', '\r', '\t'):
            # Ignore whitespace.
            pass
        elif c == '\n':
            self.line += 1
        elif c == '"':
            self.string()
        elif c.isdigit():
            self.number()
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


if __name__ == "__main__":
    Lox.main()
