import time

from exception import SuperException


class Callable:

    def arity(self) -> int:
        raise NotImplementedError()

    def call(self, interpreter: "Interpreter", arguments: list[object]) -> object:
        raise NotImplementedError()


class Clock(Callable):

    def arity(self) -> int:
        return 0

    def call(self, interpreter: "Interpreter", arguments: list[object]) -> object:
       return time.time() 

    def __str__(self) -> str:
        return "<native fn>"


class Super(Callable):

    def arity(self) -> int:
        return 3

    def call(self, interpreter: "Interpreter", arguments: list[object]) -> object:
        if arguments[0].__class__.__name__ != "LoxClass":
            raise SuperException("First argument to super() must be a Class.")
        if arguments[1].__class__.__name__ not in ("LoxClass", "Instance"):
            raise SuperException("Second argument to super() must be a Class or Instance.")
        if not isinstance(arguments[2], str):
            raise SuperException("Third argument to super() must be a string.")

        ancestor = None
        if arguments[1].__class__.__name__ == "LoxClass":
            ancestor = self.find_ancestor(arguments[0], arguments[1])
        elif arguments[1].__class__.__name__ == "Instance":
            ancestor = self.find_ancestor(arguments[0], arguments[1].klass)

        if ancestor is None:
            raise SuperException("No matching ancestor found in inheritance hierarchy.")
        inherited_method = ancestor.find_method(arguments[2])
        if inherited_method is None:
            raise SuperException("super() found no matching method.")
        return inherited_method

    def find_ancestor(self, ancestor: "LoxClass", klass: "LoxClass") -> "LoxClass":
        if klass.name == ancestor.name:
            return klass
        for superclass in klass.superclasses:
            res = self.find_ancestor(ancestor, superclass)
            if res is not None:
                return res

        return None
