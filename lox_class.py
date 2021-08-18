from function import LoxFunction
from instance import Instance
from lox_callable import Callable


class LoxClass(Callable):

    def __init__(self, name: str, superclasses: list["LoxClass"], class_methods: dict[str, LoxFunction], instance_methods: dict[str, LoxFunction], getters: dict[str, LoxFunction]):
        self.name = name
        self.superclasses = superclasses
        self.class_methods = class_methods
        self.instance_methods = instance_methods
        self.getters = getters

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
        if name in self.getters:
            return self.getters[name]
        if name in self.instance_methods:
            return self.instance_methods[name]
        class_method = self.find_class_method(name, recurse=False)
        if class_method is not None:
            return class_method
        for superclass in self.superclasses:
            res = superclass.find_method(name)
            if res is not None:
                return res
        return None

    def find_class_method(self, name: str, recurse: bool = False) -> LoxFunction:
        if name in self.class_methods:
            return self.class_methods[name].bind(self)
        if recurse:
            for superclass in self.superclasses:
                res = superclass.find_class_method(name, recurse=True)
                if res is not None:
                    return res
        return None

    def __str__(self) -> str:
        return self.name
