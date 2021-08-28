from exception import IndexException


def clean_index(index: float, max_len: int) -> int:
    index = int(index)
    if 0 <= index < max_len:
        return index
    raise IndexException("Invalid  index.")


def stringify(obj: object) -> str:
    if obj is None:
        return "nil"

    if isinstance(obj, float):
        text = str(obj)
        if text.endswith(".0"):
            text = text[:-2]
        return text

    return str(obj)
