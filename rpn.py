import expr
from token import Token
from token_type import TokenType


class ReversePolishNotation(expr.Expr.Visitor):

    def print(self, expr: expr.Expr) -> str:
        return expr.accept(self)


    def visit_binary_expr(self, expr: expr.Binary) -> str:
        return "{} {} {}".format(expr.left.accept(self), expr.right.accept(self), expr.operator.lexeme)


    def visit_grouping_expr(self, expr: expr.Grouping) -> str:
        return expr.expression.accept(self)


    def visit_literal_expr(self, expr: expr.Literal) -> str:
        if expr.value == None:
            return "nil"
        return str(expr.value)


    def visit_unary_expr(self, expr: expr.Unary) -> str:
        return "{}{}".format(expr.operator.lexeme, expr.right.accept(self))


if __name__ == "__main__":
    # (1 + 2) * (4 - 3)
    expression = expr.Binary(
        expr.Grouping(
            expr.Binary(
                expr.Literal(1),
                Token(TokenType.PLUS, "+", None, 1),
                expr.Literal(2))),
        Token(TokenType.STAR, "*", None, 1),
        expr.Grouping(
            expr.Binary(
                expr.Literal(4),
                Token(TokenType.MINUS, "-", None, 1),
                expr.Literal(3))))

    # Expect "1 2 + 4 3 - *"
    print(ReversePolishNotation().print(expression))
