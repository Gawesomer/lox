import expr
from token import Token
from token_type import TokenType


class ASTPrinter(expr.Expr.Visitor):

    def print(self, expr: expr.Expr) -> str:
        return expr.accept(self)


    def visit_binary_expr(self, expr: expr.Binary) -> str:
        return self.parenthesize(expr.operator.lexeme, expr.left, expr.right)


    def visit_grouping_expr(self, expr: expr.Grouping) -> str:
        return self.parenthesize("group", expr.expression)


    def visit_literal_expr(self, expr: expr.Literal) -> str:
        if expr.value == None:
            return "nil"
        return str(expr.value)


    def visit_unary_expr(self, expr: expr.Unary) -> str:
        return self.parenthesize(expr.operator.lexeme, expr.right)


    def parenthesize(self, name: str, *exprs) -> str:
        res = "({}".format(name)
        for expr in exprs:
            res += " {}".format(expr.accept(self))
        res += ")"
        return res


if __name__ == "__main__":
    # -123 * (45.67)
    expression = expr.Binary(
        expr.Unary(
            Token(TokenType.MINUS, "-", None, 1),
            expr.Literal(123)),
        Token(TokenType.STAR, "*", None, 1),
        expr.Grouping(
            expr.Literal(45.67)))

    # Expect "(* (- 123) (group 45.67))"
    print(ASTPrinter().print(expression))
