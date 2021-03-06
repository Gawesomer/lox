from environment import Environment
from exception import ReturnException
from instance import Instance
from lox_callable import Callable
from stmt import Function


class LoxFunction(Callable):

    def __init__(self, declaration: Function, closure: Environment, is_initializer: bool = False, is_getter: bool = False):
        self.declaration = declaration
        self.closure = closure
        self.is_initializer = is_initializer
        self.is_getter = is_getter

    def arity(self) -> int:
        return len(self.declaration.params)

    def call(self, interpreter: "Interpreter", arguments: list[object]) -> object:
        environment = Environment(self.closure)
        for i in range(len(self.declaration.params)):
            environment.initialize(self.declaration.params[i].lexeme, arguments[i])

        try:
            interpreter.execute_block(self.declaration.body, environment)
        except ReturnException as return_value:
            if self.is_initializer:
                return self.closure.get_at_no_check(0, "this")
            return return_value.value

        if self.is_initializer:
            return self.closure.get_at_no_check(0, "this")

    def bind(self, instance: [Instance, "LoxClass"]) -> "LoxFunction":
        environment = Environment(self.closure)
        environment.initialize("this", instance)
        return LoxFunction(self.declaration, environment, self.is_initializer, self.is_getter)

    def __str__(self) -> str:
        if self.declaration.name is not None:
            return "<fn {}>".format(self.declaration.name.lexeme)
        return "<fn -lambda->"
