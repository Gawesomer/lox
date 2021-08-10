from instance import Instance
from lox_callable import Callable


class LoxClass(Callable):

    def __init__(self, name: str):
        self.name = name

    def arity(self) -> int:
        return 0

    def call(self, interpreter: "Interpreter", arguments: list[object]) -> object:
        instance = Instance(self)
        return instance

    def __str__(self) -> str:
        return self.name
