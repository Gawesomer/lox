import sys

from ast_printer import ASTPrinter
from interpreter import Interpreter
from parser import Parser
from runtime_exception import RuntimeException
from scanner import Scanner
from token import Token
from token_type import TokenType


class Lox:

    had_error = False
    had_runtime_error = False

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
        _interpreter = Interpreter(cls)
        with open(filename, 'r') as f:
            code = f.read()
            cls.run(code, _interpreter)
        if cls.had_error:
            exit(65)
        if cls.had_runtime_error:
            exit(70)


    @classmethod
    def run_prompt(cls):
        _interpreter = Interpreter(cls, is_repl=True)
        while True:
            try:
                line = input("> ")
                cls.run(line, _interpreter)
                cls.had_error = False
            except EOFError:
                break


    @classmethod
    def run(cls, source: str, _interpreter: Interpreter):
        scanner = Scanner(cls, source)
        tokens = scanner.scan_tokens()
        parser = Parser(cls, tokens)
        statements = parser.parse()

        # Stop if there was a syntax error.
        if cls.had_error:
            return

        _interpreter.interpret(statements)


    @classmethod
    def error(cls, line: int, message: str):
        cls.report(line, "", message)


    @classmethod
    def report(cls, line: int, where: str, message: str):
        print("[line {}] Error{}: {}".format(line, where, message), file=sys.stderr)
        cls.had_error = True


    @classmethod
    def parse_error(cls, token: Token, message: str):
        if token.type == TokenType.EOF:
            cls.report(token.line, " at end", message)
        else:
            cls.report(token.line, " at '{}'".format(token.lexeme), message)


    @classmethod
    def runtime_error(cls, error: RuntimeException):
        print("{}\n[line {}]".format(str(error), error.token.line))
        cls.had_runtime_error = True


if __name__ == "__main__":
    Lox.main()
