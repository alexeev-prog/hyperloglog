import hashlib
import math


def get_64bit_hash(s: str) -> int:
    """64-битный хеш через первые 16 hex-цифр SHA-256"""
    full = hashlib.sha256(s.encode()).hexdigest()
    return int(full[:16], 16)


class HyperLogLog:
    def __init__(self, p: int = 14):
        """
        p: точность, число бит для индексации регистров.
        m = 2^p — количество регистров.
        """
        self.p = p
        self.m = 1 << p
        self.registers = [0] * self.m

        # Константа alpha
        if self.m == 16:
            self.alpha = 0.673
        elif self.m == 32:
            self.alpha = 0.697
        elif self.m == 64:
            self.alpha = 0.709
        else:
            self.alpha = 0.7213 / (1 + 1.079 / self.m)

    def _rho(self, hash_val: int) -> int:
        """
        Считает позицию первой единицы в хвосте хеша.
        Возвращает число от 1 до 64 - p + 1.
        """
        # Убираем p старших бит, оставляем хвост
        tail_bits = 64 - self.p
        mask = (1 << tail_bits) - 1
        tail = hash_val & mask

        # Считаем начальные нули в хвосте
        # Если tail == 0, то все биты нули -> вернём tail_bits + 1
        if tail == 0:
            return tail_bits + 1

        # Иначе считаем нули
        zeros = 0
        # Проверяем старший бит хвоста (самый левый)
        test_bit = 1 << (tail_bits - 1)
        while (tail & test_bit) == 0:
            zeros += 1
            test_bit >>= 1

        return zeros + 1

    def add(self, item: str):
        h = get_64bit_hash(item)
        index = h >> (64 - self.p)  # первые p бит
        rho = self._rho(h)
        if rho > self.registers[index]:
            self.registers[index] = rho

    def estimate(self) -> float:
        # Сумма 2^(-register)
        sum_inv = 0.0
        zero_registers = 0
        for r in self.registers:
            sum_inv += 2.0 ** (-r)
            if r == 0:
                zero_registers += 1

        raw_estimate = self.alpha * self.m * self.m / sum_inv

        # Поправка для малых кардинальностей
        if raw_estimate <= 2.5 * self.m:
            if zero_registers > 0:
                # Linear counting
                raw_estimate = self.m * math.log(self.m / zero_registers)

        # Поправка для очень больших (пропустим для простоты)
        return raw_estimate


# Тестирование
if __name__ == "__main__":
    hll = HyperLogLog(p=14)  # 16384 регистра, ~12 КБ

    # Генерируем много уникальных строк
    n_true = 100_000
    print(f"Добавляем {n_true} уникальных элементов...")
    for i in range(n_true):
        # Делаем строку из номера, чтобы не расходовать память на хранение
        hll.add(f"user_{i}")

    estimate = hll.estimate()
    error = abs(estimate - n_true) / n_true * 100
    print(f"Истинное количество: {n_true}")
    print(f"Оценка HLL:          {estimate:.0f}")
    print(f"Ошибка:              {error:.4f}%")
    print(
        f"Размер регистров:    {len(hll.registers)} чисел по ~6 бит ≈ "
        f"{len(hll.registers) * 6 / 8 / 1024:.2f} КБ"
    )
