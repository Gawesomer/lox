import typing
from enum import Enum, auto

from expr import Assign, Binary, Call, Expr, Grouping, Lambda, Literal, Logical, Ternary, Unary, Variable
from interpreter import Interpreter
from stmt import Block, Break, Class, Expression, Function, If, Print, Return, Stmt, Var, While
from lox_token import Token


class FunctionType(Enum):
    NONE = auto()
    FUNCTION = auto()


class Resolver(Expr.Visitor, Stmt.Visitor):

    def __init__(self, interpreter: Interpreter):
        super().__init__()
        self.interpreter = interpreter
        self.scopes = []
        self.current_function = FunctionType.NONE
        self.current_loop = False

    def visit_assign_expr(self, expr: Assign) -> object:
        self.resolve(expr.value)
        self.resolve_local(expr, expr.name)

    def visit_binary_expr(self, expr: Binary) -> object:
        self.resolve(expr.left)
        self.resolve(expr.right)

    def visit_call_expr(self, expr: Call) -> object:
        self.resolve(expr.callee)

        for argument in expr.arguments:
            self.resolve(argument)

    def visit_grouping_expr(self, expr: Grouping) -> object:
        self.resolve(expr.expression)

    def visit_lambda_expr(self, expr: Lambda) -> object:
        self.resolve_function(expr, FunctionType.FUNCTION)

    def visit_literal_expr(self, expr: Literal) -> object:
        return None

    def visit_logical_expr(self, expr: Logical) -> object:
        self.resolve(left)
        self.resolve(right)

    def visit_ternary_expr(self, expr: Ternary) -> object:
        self.resolve(expr.conditional)
        self.resolve(expr.truthy)
        self.resolve(expr.falsy)

    def visit_unary_expr(self, expr: Unary) -> object:
        self.resolve(expr.right)

    def visit_variable_expr(self, expr: Variable) -> object:
        if len(self.scopes) != 0 and expr.name.lexeme in self.scopes[-1] and self.scopes[-1][expr.name.lexeme]['is_defined'] == False:
            self.interpreter.reporter.parse_error(expr.name, "Can't read local variable in its own initializer.")

        self.resolve_local(expr, expr.name)

    def visit_block_stmt(self, stmt: Block):
        self.begin_scope()
        self.resolve_statements(stmt.statements)
        self.end_scope()

    def visit_break_stmt(self, stmt: Break):
        if not self.current_loop:
            self.interpreter.reporter.parse_error(stmt.keyword, "Break statement outside of enclosing loop.")
        return None

    def visit_class_stmt(self, stmt: Class):
        self.declare(stmt.name)
        self.define(stmt.name)

    def visit_expression_stmt(self, stmt: Expression):
        self.resolve(stmt.expression)

    def visit_function_stmt(self, stmt: Function):
        self.declare(stmt.name)
        self.define(stmt.name)

        self.resolve_function(stmt, FunctionType.FUNCTION)

    def resolve_function(self, function: typing.Union[Function, Lambda], _type: FunctionType):
        enclosing_function = self.current_function
        self.current_function = _type

        self.begin_scope()
        for param in function.params:
            self.declare(param)
            self.define(param)
        self.resolve_statements(function.body)
        self.end_scope()
        self.current_function = enclosing_function

    def visit_if_stmt(self, stmt: If):
        self.resolve(stmt.condition)
        self.resolve(stmt.then_branch)
        if stmt.else_branch is not None:
            self.resolve(stmt.else_branch)

    def visit_print_stmt(self, stmt: Print):
        self.resolve(stmt.expression)

    def visit_return_stmt(self, stmt: Return):
        if self.current_function == FunctionType.NONE:
            self.interpreter.reporter.parse_error(stmt.keyword, "Can't return from top-level code.")

        if stmt.value is not None:
            self.resolve(stmt.value)

    def visit_var_stmt(self, stmt: Var):
        self.declare(stmt.name)
        if stmt.initializer is not None:
            self.resolve(stmt.initializer)
        self.define(stmt.name)

    def visit_while_stmt(self, stmt: While):
        self.resolve(stmt.condition)

        self.enclosing_loop = self.current_loop
        self.current_loop = True
        self.resolve(stmt.body)
        self.current_loop = self.enclosing_loop

    def declare(self, name: Token):
        if len(self.scopes) == 0:
            return

        scope = self.scopes[-1]
        if name.lexeme in scope:
            self.interpreter.reporter.parse_error(name, "Already a variable with this name in this scope.")
        scope[name.lexeme] = {'is_defined': False, 'token': name}

    def define(self, name: Token):
        if len(self.scopes) == 0:
            return

        self.scopes[-1][name.lexeme]['is_defined'] = True

    def resolve_statements(self, statements: list[Stmt]):
        for statement in statements:
            self.resolve(statement)

    def resolve(self, item: typing.Union[Expr, Stmt]):
        item.accept(self)

    def resolve_local(self, expr: Expr, name: Token):
        for i in range(len(self.scopes)-1, -1, -1):
            if name.lexeme in self.scopes[i]:
                self.interpreter.resolve(expr, len(self.scopes)-1-i)
                return

    def begin_scope(self):
        self.scopes.append(dict())

    def end_scope(self):
        scope = self.scopes.pop()
        used_locals = {expr.name.lexeme for expr in self.interpreter.locals.keys()}
        for var in set(scope.keys()).difference(used_locals):
            self.interpreter.reporter.parse_error(scope[var]['token'], "Unused local variable {}.".format(var))
