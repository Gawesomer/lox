# Generated by gen_ast.py
from expr import Expr, Variable
from lox_token import Token


class Stmt:
    class Visitor:
        def visit_block_stmt(self, stmt: 'Block'):
            raise NotImplementedError

        def visit_class_stmt(self, stmt: 'Class'):
            raise NotImplementedError

        def visit_break_stmt(self, stmt: 'Break'):
            raise NotImplementedError

        def visit_expression_stmt(self, stmt: 'Expression'):
            raise NotImplementedError

        def visit_function_stmt(self, stmt: 'Function'):
            raise NotImplementedError

        def visit_if_stmt(self, stmt: 'If'):
            raise NotImplementedError

        def visit_print_stmt(self, stmt: 'Print'):
            raise NotImplementedError

        def visit_return_stmt(self, stmt: 'Return'):
            raise NotImplementedError

        def visit_var_stmt(self, stmt: 'Var'):
            raise NotImplementedError

        def visit_while_stmt(self, stmt: 'While'):
            raise NotImplementedError


    def accept(self, visitor):
        raise NotImplementedError


class Block(Stmt):
    def __init__(self, statements: list[Stmt]):
        self.statements = statements

    def accept(self, visitor):
        return visitor.visit_block_stmt(self)


class Class(Stmt):
    def __init__(self, name: Token, superclass: Variable, class_methods: list['Function'], instance_methods: list['Function'], getters: list['Function']):
        self.name = name
        self.superclass = superclass
        self.class_methods = class_methods
        self.instance_methods = instance_methods
        self.getters = getters

    def accept(self, visitor):
        return visitor.visit_class_stmt(self)


class Break(Stmt):
    def __init__(self, keyword: Token):
        self.keyword = keyword

    def accept(self, visitor):
        return visitor.visit_break_stmt(self)


class Expression(Stmt):
    def __init__(self, expression: Expr):
        self.expression = expression

    def accept(self, visitor):
        return visitor.visit_expression_stmt(self)


class Function(Stmt):
    def __init__(self, name: Token, params: list[Token], body: list[Stmt]):
        self.name = name
        self.params = params
        self.body = body

    def accept(self, visitor):
        return visitor.visit_function_stmt(self)


class If(Stmt):
    def __init__(self, condition: Expr, then_branch: Stmt, else_branch: Stmt):
        self.condition = condition
        self.then_branch = then_branch
        self.else_branch = else_branch

    def accept(self, visitor):
        return visitor.visit_if_stmt(self)


class Print(Stmt):
    def __init__(self, expression: Expr):
        self.expression = expression

    def accept(self, visitor):
        return visitor.visit_print_stmt(self)


class Return(Stmt):
    def __init__(self, keyword: Token, value: Expr):
        self.keyword = keyword
        self.value = value

    def accept(self, visitor):
        return visitor.visit_return_stmt(self)


class Var(Stmt):
    def __init__(self, name: Token, initializer: Expr):
        self.name = name
        self.initializer = initializer

    def accept(self, visitor):
        return visitor.visit_var_stmt(self)


class While(Stmt):
    def __init__(self, condition: Expr, body: Stmt):
        self.condition = condition
        self.body = body

    def accept(self, visitor):
        return visitor.visit_while_stmt(self)


