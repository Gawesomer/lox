from lox_token import Token
from exception import RuntimeException


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

    def assign_at(self, distance: int, name: Token, value: object):
        self.ancestor(distance).values[name.lexeme] = value

    def get(self, name: Token) -> object:
        self.check_initialized(name)
        if name.lexeme in self.values:
            return self.values[name.lexeme]

        if self.enclosing is not None:
            return self.enclosing.get(name)

        raise RuntimeException(name, "Undefined variable '{}'.".format(name.lexeme))

    def get_at(self, distance: int, name: Token) -> object:
        ancestor = self.ancestor(distance)
        ancestor.check_initialized(name)
        return ancestor.values[name.lexeme]

    def get_at_no_check(self, distance: int, name: str) -> object:
        ancestor = self.ancestor(distance)
        return ancestor.values[name]

    def check_initialized(self, name: Token):
        if name.lexeme in self.defined and name.lexeme not in self.values:
            raise RuntimeException(name, "Accessing uninitialized variable '{}'.".format(name.lexeme))

    def ancestor(self, distance: int) -> "Environment":
        environment = self
        for i in range(distance):
            environment = environment.enclosing
        return environment
