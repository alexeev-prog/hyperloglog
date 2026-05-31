import hashlib


def get_32bit_hash(s: str) -> int:
    full_hash = hashlib.sha256(s.encode()).hexdigest()

    return int(full_hash[:8], 16)  # Берём первые 8 шестнадцатеричных цифр (32 бита)


def to_binary(x: int) -> str:
    return format(x, "032b")


# примеры
words = ["hello", "world", "habr", "hyperloglog", "test"]
print(f"{'word':15s} -> {'hash':10s} -> binary")
print("-" * 39)
for w in words:
    h = get_32bit_hash(w)
    print(f"{w:15s} -> {h:10d} -> {to_binary(h)}")
