import numpy as np
import siphash
import functools
import random

from dataclasses import dataclass
from typing import Any, List, Tuple

def displace(f1: np.uint32, f2: np.uint32, d1: np.uint32, d2: np.uint32) -> np.uint32:
    return d2 + f1 * d1 + f2

DEFAULT_LAMBDA = 5

@dataclass
class Hashes:
    g: np.uint32
    f1: np.uint32
    f2: np.uint32

@dataclass
class HashState:
    key: int
    disps: List[Tuple[int, int]]
    idx_map: List[int]

def ikey_to_bkey(key: int):
    return (key << 8 * 8).to_bytes(length=16, byteorder='little')

def my_hash(x: bytes, key: int) -> Hashes: 
    bkey = ikey_to_bkey(key)
    hasher = siphash.siphash24(bkey)
    hasher.update(x)
    result = hasher.hash()
    lower = result & 0xffffffff
    upper = (result >> 32) & 0xffffffff
    # print(f"0x{result:x}")

    h = Hashes(
        g=np.uint32(lower >> 16),
        f1=np.uint32(lower),
        f2=np.uint32(upper),
    )
    return h

@dataclass
class Bucket:
    idx: int
    keys: List[int]

NP_ZERO = np.uint32(0)

def to_bytes(any: Any) -> bytes:
    match any:
        case int():
            return any.to_bytes(4, byteorder='little')
        case str():
            return any.encode()
        case bytes():
            return any
        case _:
            raise Exception(f'Object of type {type(any).__name__!r} is not hashable to bytes')

def generate_hash_state(entries: List[Any]) -> HashState:
    random.seed(1234567890)
    generator = Generator(len(entries))
    while True:
        key = random.getrandbits(64)
        hashes = [my_hash(to_bytes(entry), key) for entry in entries]

        generator.reset(hashes)
        if generator.try_generate_hash():
            return HashState(
                key=key,
                disps=generator.disps,
                idx_map=generator.map
            )

class Generator:
    def __init__(self, table_len: int) -> None:
        self.hashes: List[Hashes] = [Hashes(NP_ZERO, NP_ZERO, NP_ZERO) for _ in range(table_len)]
        self.hashes.clear()

        buckets_len = (table_len + DEFAULT_LAMBDA - 1) // DEFAULT_LAMBDA
        self.buckets: List[Bucket] = [Bucket(idx=i, keys=[]) for i in range(0, buckets_len)]
        self.disps: List[Tuple[int, int]] = [(0, 0) for _ in range(buckets_len)]
        self.map: List[None | int] = [None for _ in range(table_len)]
        self.try_map: List[int] = [0 for _ in range(table_len)]

    def reset(self, hashes: List[Hashes]):
        for bucket in self.buckets:
            bucket.keys.clear()
        self.buckets.sort(key=lambda b: b.idx)

        buckets_len = len(self.buckets)
        self.disps = [(0, 0) for _ in range(buckets_len)]
        self.map = [None for _ in range(len(self.map))]
        self.try_map = [0 for _ in range(len(self.try_map))]

        self.hashes.clear()
        self.hashes.extend(hashes)

    def gen_2(self, 
            bucket: Bucket,
            values_to_add: List[Tuple[int, int]],
            d1: int, d2: int,
            table_len: int, 
            generation: int) -> bool:
        for key in bucket.keys:
            idx = int(displace(
                    self.hashes[key].f1,
                    self.hashes[key].f2,
                    np.uint32(d1), 
                    np.uint32(d2))) % table_len
            if self.map[idx] != None or self.try_map[idx] == generation:
                return False
            self.try_map[idx] = generation
            values_to_add.append((idx, key))
        return True

    def gen_1(self, bucket: Bucket, table_len: int, values_to_add: List[Tuple[int, int]], generation: int) -> Tuple[int, bool]: 
        for d1 in range(0, table_len):
            for d2 in range(0, table_len):
                values_to_add.clear()
                generation += 1

                if not self.gen_2(bucket, values_to_add, d1, d2, table_len, generation):
                    continue
                
                self.disps[bucket.idx] = (d1, d2)
                for idx, key in values_to_add:
                    self.map[idx] = key

                return generation, True
        return generation, False

    def try_generate_hash(self): 
        buckets_len = len(self.buckets)
        for i, hash in enumerate(self.hashes):
            self.buckets[hash.g % buckets_len].keys.append(i);

        self.buckets.sort(key=functools.cmp_to_key(lambda a,b: len(b.keys) - len(a.keys)))

        generation = 0
        table_len = len(self.hashes)
        values_to_add = []

        for bucket in self.buckets:
            generation, success = self.gen_1(bucket, table_len, values_to_add, generation)
            if not success:
                return False

        return True

def format_to_string_literal(string):
    if isinstance(string, str):
        string = string.encode()
    escaped_bytes = bytearray()
    for byte in string:
        if byte == ord("'"):
            escaped_bytes.append(byte)
        elif byte == ord('"'):
            escaped_bytes.extend([ord('\\'), byte])
        elif 32 <= byte <= 126:
            escaped_bytes.append(byte)
        else:
            escaped_bytes.extend([ord('\\'), ord('x')])
            escaped_bytes.extend(f'{byte:02x}'.encode())

    return '"{}"'.format(escaped_bytes.decode('utf-8'))

def format_key(state):
    byte_string = ikey_to_bkey(state.key)
    return format_to_string_literal(byte_string)

def init_enum(enum):
    entries = [variant.data() for variant in enum]
    state = generate_hash_state(entries)
    state.__enum__ = enum
    return state

def init_set(set):
    entries = list(set) 
    state = generate_hash_state(entries)
    state.__elements__ = entries
    return state

def iter_entries(state):
    if (enum := getattr(state, '__enum__', None)) is None:
        raise RuntimeError('HashState must have assoicated Enum')
    for idx in state.idx_map:
        variant = enum[idx]
        yield variant.display(), variant.name

def iter_elements(state):
    if (elements := getattr(state, '__elements__', None)) is None:
        raise RuntimeError('HashState must have assoicated Set')
    for idx in state.idx_map:
        variant = elements[idx]
        if isinstance(variant, (str, bytes)):
            yield format_to_string_literal(variant)
        elif isinstance(variant, (int, bool)):
            yield str(variant).lower()
        else:
            raise RuntimeError('phf_set macro only supports str, bytes, int or bool elements')

#          0    1   2   3  4   5  6
entries = [12, 42, 81, 11, 3, 25, 88]
key = 10757869821655898960
disps = [(0, 2), (2, 0)]
mapping = [1, 4, 3, 6, 5, 2, 0]

def get_index(item, key):
    hashes = my_hash(to_bytes(item), key)
    d1, d2 = disps[hashes.g % len(disps)]
    return displace(hashes.f1, hashes.f2, d1, d2) % len(mapping)

if __name__ == '__main__':
    # print(get_index(81, key))
    # print(format_bytes_as_c_string(ikey_to_bkey(key)))
    # my_hash(to_bytes(12), key)
    print(generate_hash_state(entries))
    # for idx in mapping:
    #     print(f'{entries[idx], idx},')
