import expr
from token import Token
from token_type import TokenType


class Parser:

    def __init__(self, tokens: list[Token]):
        self.tokens = tokens
        self.current = 0


    def expression(self) -> expr.Expr:
        return self.equality()


    def equality(self) -> expr.Expr:
        expr = self.comparison()

        while self.match(TokenType.BANG_EQUAL, TokenType.EQUAL_EQUAL):
            operator = self.previous()
            right = self.comparison()
            expr = expr.Binary(expr, operator, right)

        return expr


    def match(self, *types) -> bool:
        for type_ in types:
            if self.check(type_):
                self.advance()
                return True

        return False


    def check(self, token_type: TokenType) -> bool:
        if self.is_at_end():
            return False
        return self.peek().type == token_type


    def advance(self) -> Token:
        if not self.is_at_end():
            self.current += 1
        return self.previous()


    def is_at_end(self) -> bool:
        return self.peek().type == TokenType.EOF


    def peek(self) -> Token:
        return self.tokens[self.current]


    def previous(self) -> Token:
        return self.tokens[self.current-1]
