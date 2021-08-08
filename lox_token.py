from token_type import TokenType


class Token:

    def __init__(self, token_type: TokenType, lexeme: str, literal, line: int):
        self.type = token_type
        self.lexeme = lexeme
        self.literal = literal
        self.line = line

    def __str__(self):
        return "{} {} {}".format(self.type, self.lexeme, self.literal)
