from lox_callable import Callable, Clock
from environment import Environment
from expr import Assign, Binary, Call, Expr, Grouping, Literal, Logical, Ternary, Unary, Variable
from exception import BreakUnwindStackException, RuntimeException
from function import LoxFunction
from stmt import Block, Break, Expression, Function, If, Print, Stmt, Var, While
from token import Token
from token_type import TokenType


class Interpreter(Expr.Visitor, Stmt.Visitor):

    def __init__(self, reporter: "Lox", is_repl: bool = False):
        super().__init__()
        self.reporter = reporter
        self.is_repl = is_repl
        self.globals = Environment()
        self.environment = self.globals

        self.globals.initialize("clock", Clock())

    def interpret(self, statements: list[Stmt]):
        try:
            for statement in statements:
                self.execute(statement)
        except RuntimeException as error:
            self.reporter.runtime_error(error)

    def visit_assign_expr(self, expr: Assign) -> object:
        value = self.evaluate(expr.value)
        self.environment.assign(expr.name, value)
        return value

    def visit_binary_expr(self, expr: Binary) -> object:
        left = self.evaluate(expr.left)
        right = self.evaluate(expr.right)

        if expr.operator.type == TokenType.COMMA:
            return right
        elif expr.operator.type == TokenType.EQUAL_EQUAL:
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
            if isinstance(left, float) and isinstance(right, float):
                return float(left) + float(right)
            elif isinstance(left, str) or isinstance(right, str):
                return str(left) + str(right)
            raise RuntimeException(expr.operator, "Operands must be two numbers or two strings.")
        elif expr.operator.type == TokenType.SLASH:
            self.check_number_operands(expr.operator, left, right)
            if float(right) == 0:
                raise RuntimeException(expr.operator, "Division by zero.")
            return float(left) / float(right)
        elif expr.operator.type == TokenType.STAR:
            self.check_number_operands(expr.operator, left, right)
            return float(left) * float(right)

        # Unreachable.
        return None

    def visit_call_expr(self, expr: Call) -> object:
        callee = self.evaluate(expr.callee)

        arguments = []
        for argument in expr.arguments:
            arguments.append(self.evaluate(argument))

        if not isinstance(callee, Callable):
            raise RuntimeException(expr.paren, "Can only call functions and classes.")

        function = callee
        if len(arguments) != function.arity():
            raise RuntimeException(
                expr.paren,
                "Expected {} arguments but got {}.".format(function.arity(), len(arguments))
            )
        return function.call(self, arguments)

    def visit_grouping_expr(self, expr: Grouping) -> object:
        return self.evaluate(expr.expression)

    def visit_literal_expr(self, expr: Literal) -> object:
        return expr.value

    def visit_logical_expr(self, expr: Logical) -> object:
        left = self.evaluate(expr.left)

        if expr.operator.type == TokenType.OR:
            if self.is_truthy(left):
                return left
        else:
            if not self.is_truthy(left):
                return left

        return self.evaluate(expr.right)

    def visit_ternary_expr(self, expr: Ternary) -> object:
        conditional = self.evaluate(expr.conditional)
        if conditional:
            return self.evaluate(expr.truthy)
        return self.evaluate(expr.falsy)

    def visit_unary_expr(self, expr: Unary) -> object:
        right = self.evaluate(expr.right)

        if expr.operator.type == TokenType.BANG:
            return not self.is_truthy(right)
        elif expr.operator.type == TokenType.MINUS:
            self.check_number_operand(expr.operator, right)
            return -float(right)

        # Unreachable
        return None

    def visit_variable_expr(self, expr: Variable) -> object:
        return self.environment.get(expr.name)

    def evaluate(self, expr: Expr) -> object:
        return expr.accept(self)

    def visit_block_stmt(self, stmt: Block):
        self.execute_block(stmt.statements, Environment(self.environment))

    def visit_break_stmt(self, stmt: Break):
        raise BreakUnwindStackException("Unwinding stack to break out of loop.")

    def visit_expression_stmt(self, stmt: Expression):
        value = self.evaluate(stmt.expression)
        if self.is_repl:
            print(self.stringify(value))

    def visit_function_stmt(self, stmt: Function):
        function = LoxFunction(stmt)
        self.environment.initialize(stmt.name.lexeme, function)

    def visit_if_stmt(self, stmt: If):
        if self.is_truthy(self.evaluate(stmt.condition)):
            self.execute(stmt.then_branch)
        elif stmt.else_branch is not None:
            self.execute(stmt.else_branch)

    def visit_print_stmt(self, stmt: Print):
        value = self.evaluate(stmt.expression)
        print(self.stringify(value))

    def visit_var_stmt(self, stmt: Var):
        if stmt.initializer is not None:
            value = self.evaluate(stmt.initializer)
            self.environment.initialize(stmt.name.lexeme, value)
        self.environment.define(stmt.name.lexeme)

    def visit_while_stmt(self, stmt: While):
        while self.is_truthy(self.evaluate(stmt.condition)):
            try:
                self.execute(stmt.body)
            except BreakUnwindStackException:
                return

    def execute(self, stmt: Stmt):
        stmt.accept(self)

    def execute_block(self, statements: list[Stmt], environment: Environment):
        previous = self.environment
        try:
            self.environment = environment
            for statement in statements:
                self.execute(statement)
        finally:
            self.environment = previous

    def is_truthy(self, obj: object) -> bool:
        if obj is None:
            return False
        if isinstance(obj, bool):
            return obj
        return True

    def is_equal(self, a: object, b: object) -> bool:
        return a == b

    def check_number_operand(self, operator: Token, operand: object):
        if isinstance(operand, float):
            return
        raise RuntimeException(operator, "Operand must be a number.")

    def check_number_operands(self, operator: Token, left: object, right: object):
        if isinstance(left, float) and isinstance(right, float):
            return
        raise RuntimeException(operator, "Operands must be numbers.")

    def stringify(self, obj: object) -> str:
        if obj is None:
            return "nil"

        if isinstance(obj, float):
            text = str(obj)
            if text.endswith(".0"):
                text = text[:-2]
            return text

        return str(obj)
