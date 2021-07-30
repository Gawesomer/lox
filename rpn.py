from expr import Binary, Expr, Grouping, Literal, Unary, Ternary
from token import Token
from token_type import TokenType


class ReversePolishNotation(Expr.Visitor):

    def print(self, expr: Expr) -> str:
        return expr.accept(self)

    def visit_binary_expr(self, expr: Binary) -> str:
        return "{} {} {}".format(expr.left.accept(self), expr.right.accept(self), expr.operator.lexeme)

    def visit_grouping_expr(self, expr: Grouping) -> str:
        return expr.expression.accept(self)

    def visit_literal_expr(self, expr: Literal) -> str:
        if expr.value is None:
            return "nil"
        return str(expr.value)

    def visit_unary_expr(self, expr: Unary) -> str:
        return "{}{}".format(expr.operator.lexeme, expr.right.accept(self))


if __name__ == "__main__":
    # (1 + 2) * (4 - 3)
    expression = Binary(
        Grouping(
            Binary(
                Literal(1),
                Token(TokenType.PLUS, "+", None, 1),
                Literal(2))),
        Token(TokenType.STAR, "*", None, 1),
        Grouping(
            Binary(
                Literal(4),
                Token(TokenType.MINUS, "-", None, 1),
                Literal(3))))

    # Expect "1 2 + 4 3 - *"
    print(ReversePolishNotation().print(expression))
