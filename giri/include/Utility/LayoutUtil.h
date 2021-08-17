class LayoutUtil {
public:
  static unsigned char* get_aligned_addr_64(unsigned char* p) {
    return (unsigned char*) (((uint64_t)p) & CACHELINE_MASK);
  }

  static unsigned char* get_aligned_addr_8(unsigned char* p) {
    return (unsigned char*) (((uint64_t)p) & WORD_MASK);
  }

  const static uint64_t CACHELINE_SIZE = 64;
  const static uint64_t CACHELINE_MASK = ~(CACHELINE_SIZE - 1);

  const static uint64_t WORD_SIZE = 8;
  const static uint64_t WORD_MASK = ~(WORD_SIZE - 1);
};
