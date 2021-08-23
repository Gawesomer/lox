import os.path

from array import LoxArray
from instance import Instance
from lox_callable import Callable
from lox_class import LoxClass
from environment import Environment
from expr import Array, Assign, Binary, Call, Expr, Index, Get, Grouping, Lambda, Literal, Logical, Set, SetArray, Ternary, This, Unary, Variable
from exception import BreakUnwindStackException, IndexException, NativeException, ReturnException, RuntimeException
from function import LoxFunction
from native import ArrayCallable, Clock, Inner, Int, Length, NoOp, ReadFile
from stmt import Block, Break, Class, Expression, Function, If, Import, Print, Return, Stmt, Var, While
from lox_token import Token
from token_type import TokenType
from util import clean_index, stringify


class Interpreter(Expr.Visitor, Stmt.Visitor):

    def __init__(self, reporter: "Lox", is_repl: bool = False):
        super().__init__()
        self.reporter = reporter
        self.is_repl = is_repl
        self.globals = Environment()
        self.environment = self.globals
        self.locals = dict()

        self.globals.initialize("array", ArrayCallable())
        self.globals.initialize("clock", Clock())
        self.globals.initialize("inner", Inner())
        self.globals.initialize("int", Int())
        self.globals.initialize("len", Length())
        self.globals.initialize("noop", NoOp())
        self.globals.initialize("readfile", ReadFile())

    def interpret(self, statements: list[Stmt]):
        try:
            for statement in statements:
                self.execute(statement)
        except (IndexException, NativeException) as error:
            self.reporter.exception_error(error)
        except RuntimeException as error:
            self.reporter.runtime_error(error)

    def visit_array_expr(self, expr: Array) -> object:
        return LoxArray([self.evaluate(element) for element in expr.elements])

    def visit_assign_expr(self, expr: Assign) -> object:
        value = self.evaluate(expr.value)

        distance = self.locals.get(expr)
        if distance is not None:
            self.environment.assign_at(distance, expr.name, value)
        else:
            self.globals.assign(expr.name, value)

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

    def visit_index_expr(self, expr: Index) -> object:
        objekt = self.evaluate(expr.objekt)
        index = self.evaluate(expr.index)
        if not isinstance(index, float):
            raise RuntimeException(expr.bracket, "Index must be a number.")

        if isinstance(objekt, LoxArray):
            return objekt.get(clean_index(index, len(objekt.elements)))
        elif isinstance(objekt, str):
            return objekt[clean_index(index, len(objekt))]

        raise RuntimeException(expr.bracket, "Can only index arrays and strings.")

    def visit_get_expr(self, expr: Get) -> object:
        objekt = self.evaluate(expr.objekt)
        if isinstance(objekt, Instance):
            res = objekt.get(expr.name)
            if isinstance(res, LoxFunction) and res.is_getter:
                res = res.call(self, ())
        elif isinstance(objekt, LoxClass):
            res = objekt.find_class_method(expr.name.lexeme, recurse=True)
        else:
            raise RuntimeException(expr.name, "Only instances have properties.")

        if res is None:
            raise RuntimeException(expr.name, "Undefined property '{}'.".format(expr.name.lexeme))
        return res

    def visit_grouping_expr(self, expr: Grouping) -> object:
        return self.evaluate(expr.expression)

    def visit_lambda_expr(self, expr: Lambda) -> object:
        stmt = Function(None, expr.params, expr.body)
        function = LoxFunction(stmt, self.environment)
        return function

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

    def visit_set_expr(self, expr: Set) -> object:
        objekt = self.evaluate(expr.objekt)

        if not isinstance(objekt, Instance):
            raise RuntimeException(expr.name, "Only instances have fields.")

        value = self.evaluate(expr.value)
        objekt.set(expr.name, value)

    def visit_setarray_expr(self, expr: SetArray) -> object:
        objekt = self.evaluate(expr.objekt)

        if not isinstance(objekt, LoxArray):
            raise RuntimeException(expr.bracket, "Can only index array.")

        index = self.evaluate(expr.index)
        if not isinstance(index, float):
            raise RuntimeException(expr.bracket, "Index must be a number.")
        value = self.evaluate(expr.value)
        objekt.set(clean_index(index, len(objekt.elements)), value)

    def visit_ternary_expr(self, expr: Ternary) -> object:
        conditional = self.evaluate(expr.conditional)
        if conditional:
            return self.evaluate(expr.truthy)
        return self.evaluate(expr.falsy)

    def visit_this_expr(self, expr: This) -> object:
        return self.lookup_variable(expr.keyword, expr)

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
        return self.lookup_variable(expr.name, expr)

    def lookup_variable(self, name: Token, expr: Expr) -> object:
        distance = self.locals.get(expr)
        if distance is not None:
            return self.environment.get_at(distance, name)
        return self.globals.get(name)

    def evaluate(self, expr: Expr) -> object:
        return expr.accept(self)

    def visit_block_stmt(self, stmt: Block):
        self.execute_block(stmt.statements, Environment(self.environment))

    def visit_break_stmt(self, stmt: Break):
        raise BreakUnwindStackException("Unwinding stack to break out of loop.")

    def visit_class_stmt(self, stmt: Class):
        evaluated_superclasses = []
        for superclass in stmt.superclasses:
            evaluated_superclass = self.evaluate(superclass)
            if not isinstance(evaluated_superclass, LoxClass):
                raise RuntimeException(superclass.name, "Superclass must be a class.")
            evaluated_superclasses.append(evaluated_superclass)

        self.environment.initialize(stmt.name.lexeme, None)

        class_methods = dict()
        for method in stmt.class_methods:
            function = LoxFunction(
                method,
                self.environment,
                is_initializer=method.name.lexeme == "init"
            )
            class_methods[method.name.lexeme] = function
        instance_methods = dict()
        for method in stmt.instance_methods:
            function = LoxFunction(
                method,
                self.environment,
                is_initializer=method.name.lexeme == "init"
            )
            instance_methods[method.name.lexeme] = function
        getters = dict()
        for method in stmt.getters:
            function = LoxFunction(
                method,
                self.environment,
                is_initializer=method.name.lexeme == "init",
                is_getter=True
            )
            getters[method.name.lexeme] = function

        klass = LoxClass(stmt.name.lexeme, evaluated_superclasses, class_methods, instance_methods, getters)

        self.environment.assign(stmt.name, klass)

    def visit_expression_stmt(self, stmt: Expression):
        value = self.evaluate(stmt.expression)
        if self.is_repl:
            print(stringify(value))

    def visit_function_stmt(self, stmt: Function):
        function = LoxFunction(stmt, self.environment)
        if stmt.name is not None:
            self.environment.initialize(stmt.name.lexeme, function)

    def visit_if_stmt(self, stmt: If):
        if self.is_truthy(self.evaluate(stmt.condition)):
            self.execute(stmt.then_branch)
        elif stmt.else_branch is not None:
            self.execute(stmt.else_branch)

    def visit_import_stmt(self, stmt: Import):
        if not os.path.exists(stmt.filename.lexeme):
            raise RuntimeException(stmt.filename, "Imported filename cannot be found.")
        with open(stmt.filename.lexeme) as imported_file:
            self.reporter.run(imported_file.read(), self)

    def visit_print_stmt(self, stmt: Print):
        value = self.evaluate(stmt.expression)
        print(stringify(value))

    def visit_return_stmt(self, stmt: Return):
        value = None
        if stmt.value is not None:
            value = self.evaluate(stmt.value)

        raise ReturnException(value)

    def visit_var_stmt(self, stmt: Var):
        if stmt.initializer is not None:
            value = self.evaluate(stmt.initializer)
            self.environment.initialize(stmt.name.lexeme, value)
        else:
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

    def resolve(self, expr: Expr, depth: int):
        self.locals[expr] = depth

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
