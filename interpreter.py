from expr import Expr, Binary, Grouping, Literal, Unary, Ternary
from runtime_exception import RuntimeException
from token import Token
from token_type import TokenType


class Interpreter(Expr.Visitor):


    def __init__(self, reporter: "Lox"):
        super().__init__()
        self.reporter = reporter


    def interpret(self, expression: Expr):
        try:
            value = self.evaluate(expression)
            print(self.stringify(value))
        except RuntimeException as error:
            self.reporter.runtime_error(error)


    def visit_binary_expr(self, expr: Binary) -> object:
        left = self.evaluate(expr.left)
        right = self.evaluate(expr.right)

        if expr.operator.type == TokenType.EQUAL_EQUAL:
            return self.is_equal(left, right)
        elif expr.operator.type == TokenType.BANG_EQUAL:
            return not self.is_equal(left, right)
        elif expr.operator.type == TokenType.GREATER:
            self.check_number_operands(expr.operator, left, right)
            return float(left) > float(right)
        elif expr.operator.type == TokenType.GREATER_EQUAL:
            self.check_number_operands(expr.operator, left, right)
            return float(left) >= float(right)
        elif expr.operator.type == TokenType.LESS:
            self.check_number_operands(expr.operator, left, right)
            return float(left) < float(right)
        elif expr.operator.type == TokenType.LESS_EQUAL:
            self.check_number_operands(expr.operator, left, right)
            return float(left) <= float(right)
        elif expr.operator.type == TokenType.MINUS:
            self.check_number_operands(expr.operator, left, right)
            return float(left) - float(right)
        elif expr.operator.type == TokenType.PLUS:
            if isinstance(left, int) and isinstance(right, int):
                return float(left) + float(right)
            elif isinstance(left, str) and isinstance(right, str):
                return str(left) + str(right)
            raise RuntimeException(operator, "Operands must be two numbers or two strings.")
        elif expr.operator.type == TokenType.SLASH:
            self.check_number_operands(expr.operator, left, right)
            return float(left) / float(right)
        elif expr.operator.type == TokenType.STAR:
            self.check_number_operands(expr.operator, left, right)
            return float(left) * float(right)

        # Unreachable.
        return None


    def visit_grouping_expr(self, expr: Grouping) -> object:
        return self.evaluate(expr.expression)


    def visit_literal_expr(self, expr: Literal) -> object:
        return expr.value


    def visit_unary_expr(self, expr: Unary) -> object:
        right = self.evaluate(expr.right)

        if expr.operator.type == TokenType.BANG:
            return not self.is_truthy(right)
        elif expr.operator.type == TokenType.MINUS:
            self.check_number_operand(expr.operator, right)
            return -int(right)

        # Unreachable
        return None


    def visit_ternary_expr(self, expr: Ternary) -> object:
        conditional = self.evaluate(expr.conditional)
        if conditional:
            return self.evaluate(expr.truthy)
        return self.evaluate(expr.falsy)


    def evaluate(self, expr: Expr) -> object:
        return expr.accept(self)


    def is_truthy(self, obj: object) -> bool:
        if obj is None:
            return False
        if isinstance(obj, bool):
            return obj
        return True


    def is_equal(self, a: object, b: object) -> bool:
        return a == b


    def check_number_operand(self, operator: Token, operand: object):
        if isinstance(operand, int):
            return
        raise RuntimeException(operator, "Operand must be a number.")


    def check_number_operands(self, operator: Token, left: object, right: object):
        if isinstance(left, int) and isinstance(right, int):
            return
        raise RuntimeException(operator, "Operands must be numbers.")


    def stringify(self, obj: object) -> str:
        if obj is None:
            return "nil"

        if isinstance(obj, int):
            text = str(obj)
            if text.endswith(".0"):
                text = text[:-2]
            return text

        return str(obj)
