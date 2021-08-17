# Cacheline size in bytes
CACHELINE_BYTES = 64

# Atomic write size in bytes
ATOMIC_WRITE_BYTES = 8

# Return the cacheline start address
def get_cacheline_address(address):
    return int(address / CACHELINE_BYTES) * CACHELINE_BYTES
