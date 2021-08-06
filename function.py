from environment import Environment
from lox_callable import Callable
from stmt import Function


class LoxFunction(Callable):

    def __init__(self, declaration: Function):
        self.declaration = declaration

    def arity(self) -> int:
        return len(self.declaration.params)

    def call(self, interpreter: "Interpreter", arguments: list[object]) -> object:
        environment = Environment(interpreter.globals)
        for i in range(len(self.declaration.params)):
            environment.initialize(self.declaration.params[i].lexeme, arguments[i])

        interpreter.execute_block(self.declaration.body, environment)

    def __str__(self) -> str:
        return "<fn {}>".format(self.declaration.name.lexeme)
