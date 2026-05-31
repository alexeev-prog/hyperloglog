import hashlib
import random
import string


def get_32bit_hash(s: str) -> int:
    full_hash = hashlib.sha256(s.encode()).hexdigest()
    return int(full_hash[:8], 16)


def count_leading_zeros_in_tail(x: int, p: int) -> int:
    """
    Считаем начальные нули в битах ПОСЛЕ первых p бит.
    x — 32-битное число
    p — сколько первых бит занято под индекс корзины
    """
    # Сдвигаем, оставляя хвост
    tail = x << p  # убираем первые p бит слева (в 32-битном мире)
    tail = tail & 0xFFFFFFFF  # оставляем 32 бита
    binary = format(tail, "032b")
    count = 0
    for bit in binary:
        if bit == "0":
            count += 1
        else:
            break
    return count


class FlajoletMartin:
    def __init__(self, p: int = 5):
        self.p = p
        self.m = 1 << p  # m = 2^p
        self.registers = [0] * self.m

    def add(self, item: str):
        h = get_32bit_hash(item)
        # Первые p бит — индекс корзины
        index = h >> (32 - self.p)  # берём старшие p бит
        # Хвост хеша — считаем начальные нули + 1
        rho = count_leading_zeros_in_tail(h, self.p) + 1
        # Обновляем регистр
        if rho > self.registers[index]:
            self.registers[index] = rho

    def estimate(self) -> float:
        # Сумма 2^(-register)
        sum_inv = 0.0
        for r in self.registers:
            sum_inv += 2 ** (-r)
        # Константа alpha для данного m (приблизительная)
        # Для m=32 (p=5) alpha ~ 0.697
        if self.m == 16:
            alpha = 0.673
        elif self.m == 32:
            alpha = 0.697
        elif self.m == 64:
            alpha = 0.709
        else:
            # Для больших m alpha стремится к 0.7213 / (1 + 1.079/m)
            alpha = 0.7213 / (1 + 1.079 / self.m)

        raw = alpha * self.m * self.m / sum_inv
        return raw


def generate_unique(n: int):
    s: set[str] = set()
    while len(s) < n:
        s.add("".join(random.choices(string.ascii_letters, k=10)))
    return list(s)


# Проверим для разных n
for n in [100, 1000, 10000, 50000]:
    data = generate_unique(n)
    fm = FlajoletMartin(p=5)  # 32 корзины
    for item in data:
        fm.add(item)
    est = fm.estimate()
    error = abs(est - n) / n * 100
    print(f"n={n:6d} -> оценка={est:.0f} (ошибка {error:.2f}%)")
