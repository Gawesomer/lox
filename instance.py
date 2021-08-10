class Instance:

    def __init__(self, klass: "LoxClass"):
        self.klass = klass

    def __str__(self) -> str:
        return "{} instance".format(self.klass.name)
