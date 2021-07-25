# Generated by gen_ast.py
from expr import Expr


class Stmt:
    class Visitor:
        def visit_expression_stmt(self, stmt: 'Expression'):
            raise NotImplementedError

        def visit_print_stmt(self, stmt: 'Print'):
            raise NotImplementedError


    def accept(self, visitor):
        raise NotImplementedError


class Expression(Stmt):
    def __init__(self, expression: Expr):
        self.expression = expression

    def accept(self, visitor):
        return visitor.visit_expression_stmt(self)


class Print(Stmt):
    def __init__(self, expression: Expr):
        self.expression = expression

    def accept(self, visitor):
        return visitor.visit_print_stmt(self)


