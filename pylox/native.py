import os.path
import time

from array import LoxArray
from instance import Instance
from lox_callable import Callable
from lox_class import LoxClass
from exception import NativeException


class ArrayCallable(Callable):

    def arity(self) -> int:
        return 1

    def call(self, interpreter: "Interpreter", arguments: list[object]) -> object:
        size = arguments[0]
        if not isinstance(size, float):
            raise NativeException("array: Argument must be a number.")
        return LoxArray([None for i in range(int(size))])

    def __str__(self) -> str:
        return "<native fn: array>"


class Char(Callable):

    def arity(self) -> int:
        return 1

    def call(self, interpreter: "Interpreter", arguments: list[object]) -> object:
        number = arguments[0]

        if isinstance(number, float):
            return chr(int(number))

        raise NativeException("chr: Argument must be a number.")

    def __str__(self) -> str:
        return "<native fn: chr>"


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


class Int(Callable):

    def arity(self) -> int:
        return 1

    def call(self, interpreter: "Interpreter", arguments: list[object]) -> object:
        number = arguments[0]

        if isinstance(number, float):
            return float(int(number))
        elif isinstance(number, str) and len(number) == 1:
            return float(ord(number))

        raise NativeException("int: Argument must be a number or character.")

    def __str__(self) -> str:
        return "<native fn: int>"


class Length(Callable):

    def arity(self) -> int:
        return 1

    def call(self, interpreter: "Interpreter", arguments: list[object]) -> object:
        objekt = arguments[0]

        if objekt.__class__.__name__ != "LoxArray" and not isinstance(objekt, str):
            raise NativeException("len: Argument must be array or string.")

        return float(len(objekt))

    def __str__(self) -> str:
        return "<native fn: len>"


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


class ReadFile(Callable):

    def arity(self) -> int:
        return 1

    def call(self, interpreter: "Interpreter", arguments: list[object]) -> object:
        filename = arguments[0]

        if not isinstance(filename, str):
            raise NativeException("readfile: Argument must be a string.")

        if not os.path.exists(filename):
            raise NativeException("readfile: File cannot be found.")

        with open(filename, encoding="latin-1") as f:
            return f.read()

    def __str__(self) -> str:
        return "<native fn: readfile>"


class WriteFile(Callable):

    def arity(self) -> int:
        return 2

    def call(self, interpreter: "Interpreter", arguments: list[object]) -> object:
        filename, text = arguments

        if not isinstance(filename, str):
            raise NativeException("writefile: First argument must be a string.")
        if not isinstance(text, str):
            raise NativeException("writefile: Second argument must be a string.")

        try:
            with open(filename, "w", encoding="latin-1") as f:
                f.write(text)
        except UnicodeEncodeError:
            raise NativeException("writefile: Invalid character set. Can only handle ASCII.")

    def __str__(self) -> str:
        return "<native fn: writefile>"
