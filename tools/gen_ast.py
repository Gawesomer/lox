import sys


class GenerateAST:

    @classmethod
    def main(cls):
        if len(sys.argv) != 2:
            print("Usage: generate_ast <output directory>")
            exit(1)
        output_dir = sys.argv[1]
        cls.define_ast(output_dir, "Expr",
            [
                "from lox_token import Token",
            ],
            [
                "Array    ; elements: list[Expr]",
                "Assign   ; name: Token, value: Expr",
                "Binary   ; left: Expr, operator: Token, right: Expr",
                "Call     ; callee: Expr, paren: Token, arguments: list[Expr]",
                "Index    ; objekt: Expr, index: Expr, bracket: Token",
                "Get      ; objekt: Expr, name: Token",
                "Grouping ; expression: Expr",
                "Lambda   ; params: list[Token], body: list['Stmt']",
                "Literal  ; value: object",
                "Logical  ; left: Expr, operator: Token, right: Expr",
                "Set      ; objekt: Expr, name: Token, value: Expr",
                "SetArray ; objekt: Expr, index: Expr, value: Expr, bracket: Token",
                "Ternary  ; conditional: Expr, truthy: Expr, falsy: Expr",
                "This     ; keyword: Token",
                "Unary    ; operator: Token, right: Expr",
                "Variable ; name: Token",
            ]
        )
        cls.define_ast(output_dir, "Stmt",
            [
                "from expr import Expr, Variable",
                "from lox_token import Token",
            ],
            [
                "Block      ; statements: list[Stmt]",
                "Class      ; name: Token, superclasses: list[Variable], class_methods: list['Function'], instance_methods: list['Function'], getters: list['Function']",
                "Break      ; keyword: Token",
                "Expression ; expression: Expr",
                "Function   ; name: Token, params: list[Token], body: list[Stmt]",
                "If         ; condition: Expr, then_branch: Stmt, else_branch: Stmt",
                "Print      ; expression: Expr",
                "Return     ; keyword: Token, value: Expr",
                "Var        ; name: Token, initializer: Expr",
                "While      ; condition: Expr, body: Stmt",
            ]
        )

    @classmethod
    def define_ast(cls, output_dir: str, base_name: str, imports: list[str], types: list[str]):
        path = "{}/{}.py".format(output_dir, base_name.lower())
        with open(path, "w") as writer:
            writer.write("# Generated by gen_ast.py\n")
            # Imports.
            for line in imports:
                writer.write("{}\n".format(line))
            writer.write("\n")
            writer.write("\n")

            # Base class.
            writer.write("class {}:\n".format(base_name))

            cls.define_visitor(writer, base_name, types)

            # The base accept() method.
            writer.write("    def accept(self, visitor):\n")
            writer.write("        raise NotImplementedError\n")

            writer.write("\n")
            writer.write("\n")

            # The AST classes.
            for type_ in types:
                class_name, fields = [s.strip() for s in type_.split(";")]
                cls.define_type(writer, base_name, class_name, fields)

    @classmethod
    def define_type(cls, writer, base_name: str, class_name: str, field_list: str):
        writer.write("class {}({}):\n".format(class_name, base_name))

        # Constructor.
        writer.write("    def __init__(self, {}):\n".format(field_list))

        # Store parameters in fields.
        fields = field_list.split(", ")
        for field in fields:
            name = field.split(":")[0]
            writer.write("        self.{} = {}\n".format(name, name))

        writer.write("\n")

        # Visitor pattern.
        writer.write("    def accept(self, visitor):\n")
        writer.write("        return visitor.visit_{}_{}(self)\n".format(class_name.lower(), base_name.lower()))

        writer.write("\n")
        writer.write("\n")

    @classmethod
    def define_visitor(cls, writer, base_name: str, types: list[str]):
        writer.write("    class Visitor:\n")

        for type_ in types:
            type_name = type_.split(";")[0].strip()
            writer.write("        def visit_{}_{}(self, {}: '{}'):\n".format(type_name.lower(), base_name.lower(), base_name.lower(), type_name))
            writer.write("            raise NotImplementedError\n")
            writer.write("\n")

        writer.write("\n")


if __name__ == "__main__":
    GenerateAST.main()
