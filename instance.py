from exception import RuntimeException
from lox_token import Token


class Instance:

    def __init__(self, klass: "LoxClass"):
        self.klass = klass
        self.fields = dict()

    def get(self, name: Token) -> object:
        if name.lexeme in self.fields:
            return self.fields[name.lexeme]

        method = self.klass.find_method(name.lexeme)
        if method is not None:
            return method.bind(self)

        return None

    def set(self, name: Token, value: object):
        self.fields[name.lexeme] = value

    def __str__(self) -> str:
        return "{} instance".format(self.klass.name)
