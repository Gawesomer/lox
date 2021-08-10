from environment import Environment
from exception import ReturnException
from instance import Instance
from lox_callable import Callable
from stmt import Function


class LoxFunction(Callable):

    def __init__(self, declaration: Function, closure: Environment):
        self.declaration = declaration
        self.closure = closure

    def arity(self) -> int:
        return len(self.declaration.params)

    def call(self, interpreter: "Interpreter", arguments: list[object]) -> object:
        environment = Environment(self.closure)
        for i in range(len(self.declaration.params)):
            environment.initialize(self.declaration.params[i].lexeme, arguments[i])

        try:
            interpreter.execute_block(self.declaration.body, environment)
        except ReturnException as return_value:
            return return_value.value

    def bind(self, instance: Instance) -> "LoxFunction":
        environment = Environment(self.closure)
        environment.initialize("this", instance)
        return LoxFunction(self.declaration, environment)

    def __str__(self) -> str:
        if self.declaration.name is not None:
            return "<fn {}>".format(self.declaration.name.lexeme)
        return "<fn -lambda->"
