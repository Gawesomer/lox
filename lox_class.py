from function import LoxFunction
from instance import Instance
from lox_callable import Callable


class LoxClass(Callable):

    def __init__(self, name: str, class_methods: dict[str, LoxFunction], instance_methods: dict[str, LoxFunction]):
        self.name = name
        self.class_methods = class_methods
        self.instance_methods = instance_methods

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
        if name in self.instance_methods:
            return self.instance_methods[name]
        return self.find_class_method(name)

    def find_class_method(self, name: str) -> LoxFunction:
        if name in self.class_methods:
            return self.class_methods[name]
        return None

    def __str__(self) -> str:
        return self.name
