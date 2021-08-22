import time


class Callable:

    def arity(self) -> int:
        raise NotImplementedError()

    def call(self, interpreter: "Interpreter", arguments: list[object]) -> object:
        raise NotImplementedError()
