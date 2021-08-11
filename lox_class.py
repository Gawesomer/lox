from function import LoxFunction
from instance import Instance
from lox_callable import Callable


class LoxClass(Callable):

    def __init__(self, name: str, methods: dict[str, LoxFunction]):
        self.name = name
        self.methods = methods

    def arity(self) -> int:
        initializer = self.find_method("init")
        if initializer is None:
            return 0
        return initializer.arity()

    def call(self, interpreter: "Interpreter", arguments: list[object]) -> object:
        instance = Instance(self)
        initializer = self.find_method("init")
        if initializer is not None:
            initializer.bind(instance).call(interpreter, arguments)

        return instance

    def find_method(self, name: str) -> LoxFunction:
        if name in self.methods:
            return self.methods[name]
        return None

    def __str__(self) -> str:
        return self.name
