from token import Token
from runtime_exception import RuntimeException


class Environment:

    values = {}

    def define(self, name: str, value: object):
        self.values[name] = value

    def get(self, name: Token) -> object:
        if name.lexeme in self.values:
            return self.values[name.lexeme]

        raise RuntimeException(name, "Undefined variable '{}'.".format(name.lexeme))


    def assign(self, name: Token, value: object):
        if name.lexeme in self.values:
            self.values[name.lexeme] = value
            return

        raise RuntimeException(name, "Undefined variable '{}'.".format(name.lexeme))
