from token import Token


class BreakUnwindStackException(Exception):
    pass


class RuntimeException(Exception):

    def __init__(self, token: Token, message: str):
        super().__init__(message)
        self.token = token
