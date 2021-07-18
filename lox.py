import sys

from ast_printer import ASTPrinter
from parser import Parser
from scanner import Scanner
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
        scanner = Scanner(cls, source)
        tokens = scanner.scan_tokens()
        for token in tokens:
            print(token)
        parser = Parser(cls, tokens)
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


if __name__ == "__main__":
    Lox.main()
