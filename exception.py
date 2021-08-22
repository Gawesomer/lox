from lox_token import Token


class BreakUnwindStackException(Exception):
    pass


class IndexException(Exception):
    pass


class NativeException(Exception):
    pass


class ReturnException(Exception):

    def __init__(self, value: object, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self.value = value


class RuntimeException(Exception):

    def __init__(self, token: Token, message: str):
        super().__init__(message)
        self.token = token
