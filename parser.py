import expr
from token import Token
from token_type import TokenType


class Parser:
    """
    Expression grammar:
        expression     → inv_comma ;
        inv_comma      → "," comma ;
        comma          → inv_ternary ("," inv_ternary )* ;
        inv_ternary    → ( "?" | ":" ) ternary ;
        ternary        → inv_equality ( "?" inv_equality ":" inv_equality )* ;
        inv_equality   → ( "==" | "!=" ) equality ;
        equality       → inv_comparison ( ( "!=" | "==" ) inv_comparison )* ;
        inv_comparison → ( ">" | ">=" | "<" | "<=" ) comparison ;
        comparison     → inv_term ( ( ">" | ">=" | "<" | "<=" ) inv_term )* ;
        inv_term       → ( "-" | "+" ) term ;
        term           → inv_factor ( ( "-" | "+" ) inv_factor )* ;
        inv_factor     → ( "/" | "*" ) factory ;
        factor         → unary ( ( "/" | "*" ) unary )* ;
        unary          → ( "!" | "-" ) unary
                       | primary ;
        primary        → NUMBER | STRING | "true" | "false" | "nil"
                       | "(" expression ")" ;
    """

    class ParseError(Exception):
        pass


    def __init__(self, reporter: "Lox", tokens: list[Token]):
        self.reporter = reporter
        self.tokens = tokens
        self.current = 0


    def parse(self) -> expr.Expr:
        try:
            return self.expression()
        except self.ParseError:
            return None


    def expression(self) -> expr.Expr:
        return self.inv_comma()


    def inv_comma(self) -> expr.Expr:
        if self.match(TokenType.COMMA):
            self.error(self.peek(), "Comma operator without left-hand operand.")
            invalid_expression = self.comma()

        return self.comma()


    def comma(self) -> expr.Expr:
        expression = self.inv_ternary()

        while self.match(TokenType.COMMA):
            operator = self.previous()
            right = self.inv_ternary()
            expression = expr.Binary(expression, operator, right)

        return expression


    def inv_ternary(self) -> expr.Expr:
        if self.match(TokenType.EROTEME, TokenType.COLON):
            self.error(self.peek(), "Ternary operator without left-hand operand.")
            invalid_expression = self.ternary()

        return self.ternary()


    def ternary(self) -> expr.Expr:
        expression = self.inv_equality()

        while self.match(TokenType.EROTEME):
            truthy = self.inv_equality()
            if self.match(TokenType.COLON):
                falsy = self.inv_equality()
                expression = expr.Ternary(expression, truthy, falsy)
            else:
                raise self.error(self.peek(), "Expect ':'.")

        return expression


    def inv_equality(self) -> expr.Expr:
        if self.match(TokenType.BANG_EQUAL, TokenType.EQUAL_EQUAL):
            self.error(self.peek(), "Equality operator without left-hand operand.")
            invalid_expression = self.equality()

        return self.equality()


    def equality(self) -> expr.Expr:
        expression = self.inv_comparison()

        while self.match(TokenType.BANG_EQUAL, TokenType.EQUAL_EQUAL):
            operator = self.previous()
            right = self.inv_comparison()
            expression = expr.Binary(expression, operator, right)

        return expression


    def inv_comparison(self) -> expr.Expr:
        if self.match(TokenType.GREATER, TokenType.GREATER_EQUAL, TokenType.LESS, TokenType.LESS_EQUAL):
            self.error(self.peek(), "Comparison operator without left-hand operand.")
            invalid_expression = self.comparison()

        return self.comparison()


    def comparison(self) -> expr.Expr:
        expression = self.inv_term()

        while self.match(TokenType.GREATER, TokenType.GREATER_EQUAL, TokenType.LESS, TokenType.LESS_EQUAL):
            operator = self.previous()
            right = self.inv_term()
            expression = expr.Binary(expression, operator, right)

        return expression


    def inv_term(self) -> expr.Expr:
        if self.match(TokenType.MINUS, TokenType.PLUS):
            self.error(self.peek(), "Term operator without left-hand operand.")
            invalid_expression = self.term()

        return self.term()


    def term(self) -> expr.Expr:
        expression = self.inv_factor()

        while self.match(TokenType.MINUS, TokenType.PLUS):
            operator = self.previous()
            right = self.inv_factor()
            expression = expr.Binary(expression, operator, right)

        return expression


    def inv_factor(self) -> expr.Expr:
        if self.match(TokenType.SLASH, TokenType.STAR):
            self.error(self.peek(), "Factor operator without left-hand operand.")
            invalid_expression = self.factor()

        return self.factor()


    def factor(self) -> expr.Expr:
        expression = self.unary()

        while self.match(TokenType.SLASH, TokenType.STAR):
            operator = self.previous()
            right = self.factor()
            expression = expr.Binary(expression, operator, right)

        return expression


    def unary(self) -> expr.Expr:
        if self.match(TokenType.BANG, TokenType.MINUS):
            operator = self.previous()
            right = self.unary()
            return expr.Unary(operator, right)

        return self.primary()


    def primary(self) -> expr.Expr:
        if self.match(TokenType.FALSE):
            return expr.Literal(False)
        if self.match(TokenType.TRUE):
            return expr.Literal(True)
        if self.match(TokenType.NIL):
            return expr.Literal(None)

        if self.match(TokenType.NUMBER, TokenType.STRING):
            return expr.Literal(self.previous().literal)

        if self.match(TokenType.LEFT_PAREN):
            expression = self.expression()
            self.consume(TokenType.RIGHT_PAREN, "Expect ')' after expression.")
            return expr.Grouping(expression)

        raise self.error(self.peek(), "Expect expression.")


    def match(self, *types) -> bool:
        for type_ in types:
            if self.check(type_):
                self.advance()
                return True

        return False


    def consume(self, token_type: TokenType, message: str) -> Token:
        if self.check(token_type):
            return self.advance()

        raise self.error(self.peek(), message)


    def check(self, token_type: TokenType) -> bool:
        if self.is_at_end():
            return False
        return self.peek().type == token_type


    def check_next_next(self, token_type: TokenType) -> bool:
        if self.current+2 >= len(self.tokens):
            return False
        return self.tokens[self.current+2].type == token_type


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


    def error(self, token: Token, message: str) -> Exception:
        self.reporter.error_parser(token, message)
        return self.ParseError()


    def synchronize(self):
        self.advance()

        while not self.is_at_end():
            if self.previous().type == TokenType.SEMICOLON:
                return

            if self.peek().type in [
                TokenType.CLASS,
                TokenType.FUN,
                TokenType.VAR,
                TokenType.FOR,
                TokenType.IF,
                TokenType.WHILE,
                TokenType.PRINT,
                TokenType.RETURN]:
                return

            self.advance()
