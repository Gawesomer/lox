import time


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
