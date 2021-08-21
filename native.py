from instance import Instance
from lox_callable import Callable
from lox_class import LoxClass
from exception import NativeException


class Clock(Callable):

    def arity(self) -> int:
        return 0

    def call(self, interpreter: "Interpreter", arguments: list[object]) -> object:
       return time.time() 

    def __str__(self) -> str:
        return "<native fn: clock>"


class Inner(Callable):

    def arity(self) -> int:
        return 3

    def call(self, interpreter: "Interpreter", arguments: list[object]) -> object:
        stop_class, instance, method_name = arguments

        if stop_class.__class__.__name__ != "LoxClass":
            raise NativeException("inner: First argument must be a Class.")
        if instance.__class__.__name__ not in ("LoxClass", "Instance"):
            raise NativeException("inner: Second  argument must be a Class or Instance.")
        if not isinstance(method_name, str):
            raise NativeException("inner: Third  argument must be a string.")

        if instance.__class__.__name__ == "LoxClass":
            inner_method = instance.find_class_method(method_name, stop_at=stop_class, recurse=True)
        else:
            inner_method = instance.klass.find_method(method_name, stop_at=stop_class)

        if inner_method is None:
            return NoOp()
        return inner_method.bind(instance)

    def __str__(self) -> str:
        return "<native fn: inner>"


class NoOp(Callable):

    def arity(self) -> int:
        class AlwaysEq:
            def __eq__(self, other: object) -> bool:
                return True
        return AlwaysEq()

    def call(self, interpreter: "Interpreter", arguments: list[object]) -> object:
        return None

    def __str__(self) -> str:
        return "<native fn: noop>"
