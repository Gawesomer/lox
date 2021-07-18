from token import Token


class RuntimeException(Exception):

    def __init__(self, token: Token, message: str):
        super().__init__(message)
        self.token = token
