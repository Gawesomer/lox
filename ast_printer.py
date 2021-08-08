from expr import Expr, Binary, Grouping, Literal, Unary, Ternary
from lox_token import Token
from token_type import TokenType


class ASTPrinter(Expr.Visitor):

    def print(self, expr: Expr) -> str:
        return expr.accept(self)

    def visit_binary_expr(self, expr: Binary) -> str:
        return self.parenthesize(expr.operator.lexeme, expr.left, expr.right)

    def visit_grouping_expr(self, expr: Grouping) -> str:
        return self.parenthesize("group", expr.expression)

    def visit_literal_expr(self, expr: Literal) -> str:
        if expr.value is None:
            return "nil"
        return str(expr.value)

    def visit_unary_expr(self, expr: Unary) -> str:
        return self.parenthesize(expr.operator.lexeme, expr.right)

    def visit_ternary_expr(self, expr: Ternary) -> str:
        return self.parenthesize("ternary", expr.conditional, expr.truthy, expr.falsy)

    def parenthesize(self, name: str, *exprs) -> str:
        res = "({}".format(name)
        for expr in exprs:
            res += " {}".format(expr.accept(self))
        res += ")"
        return res


if __name__ == "__main__":
    # -123 * (45.67)
    expression = Binary(
        Unary(
            Token(TokenType.MINUS, "-", None, 1),
            Literal(123)),
        Token(TokenType.STAR, "*", None, 1),
        Grouping(
            Literal(45.67)))

    # Expect "(* (- 123) (group 45.67))"
    print(ASTPrinter().print(expression))
