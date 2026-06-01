import hashlib
import random
import string


def get_32bit_hash(s: str) -> int:
    full_hash = hashlib.sha256(s.encode()).hexdigest()
    return int(full_hash[:8], 16)


def count_leading_zeros(x: int) -> int:
    binary = format(x, "032b")
    count = 0
    for bit in binary:
        if bit == "0":
            count += 1
        else:
            break
    return count


# генерим много случайных строк
random.seed(42)
unique_strings = set()
for _ in range(10000):
    s = "".join(random.choices(string.ascii_letters, k=10))
    unique_strings.add(s)

unique_strings = list(unique_strings)  # type: ignore
print(f"Реальное количество уникальных: {len(unique_strings)}")

# найдем максимальное число начальных нулей
max_zeros = 0
for s in unique_strings:
    h = get_32bit_hash(s)
    zeros = count_leading_zeros(h)
    if zeros > max_zeros:
        max_zeros = zeros

estimate = 2**max_zeros
print(f"Максимум начальных нулей: {max_zeros}")
print(f"Оценка по 2^max_zeros: {estimate}")
