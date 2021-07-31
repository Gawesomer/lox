from token import Token
from runtime_exception import RuntimeException


class Environment:

    def __init__(self, enclosing=None):
        self.enclosing = enclosing
        self.defined = set()
        self.values = dict()

    def define(self, name: str):
        self.defined.add(name)

    def initialize(self, name: str, value: object):
        self.define(name)
        self.values[name] = value

    def assign(self, name: Token, value: object):
        if name.lexeme in self.defined:
            self.values[name.lexeme] = value
            return

        if self.enclosing is not None:
            self.enclosing.assign(name, value)
            return

        raise RuntimeException(name, "Undefined variable '{}'.".format(name.lexeme))

    def get(self, name: Token) -> object:
        if name.lexeme in self.values:
            return self.values[name.lexeme]
        elif name.lexeme in self.defined:
            raise RuntimeException(name, "Accessing uninitialized variable '{}'.".format(name.lexeme))

        if self.enclosing is not None:
            return self.enclosing.get(name)

        raise RuntimeException(name, "Undefined variable '{}'.".format(name.lexeme))
