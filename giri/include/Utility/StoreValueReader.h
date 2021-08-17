#include <sys/stat.h>
#include <sys/mman.h>
#include <unistd.h>

//===----------------------------------------------------------------------===//
//                        Trace Store Value Reader
//===----------------------------------------------------------------------===//
class StoreValueReader {
public:
  /// Open the file descriptor and mmap the StoreValueCacheBytes to the cache
  void init(int fd);

  /// get the next value (len bytes) and store it into dest
  void getNextValue(unsigned char *dest, uintptr_t len);

  /// skip this value and move forward the currOffset
  void moveCurrOffset(uintptr_t len);

  /// munmap
  void close();

private:
  /// Map the trace file to cache
  void mapCache(void);
  /// Update the currOffset if it encounter a Cache boundary
  void checkCacheBoundaryAndUpdateCurrOffset(uintptr_t len);

private:
  uintptr_t currOffset; ///< first available addr: cache+currOff
  unsigned char *values; ///< A cache that needs to be written to disk
  size_t length;

  unsigned long StoreValueCacheBytes; ///< Size of the cache in bytes
  static const float LOAD_FACTOR; ///< load factor of the system memory
};

const float StoreValueReader::LOAD_FACTOR = 0.1;

void StoreValueReader::init(int fd) {
  // Attempt to get the file size.
  struct stat finfo;
  int ret = fstat(fd, &finfo);
  assert((ret == 0) && "Cannot fstat() file!\n");

  // Note that we map the whole file in the private memory space. If we don't
  // have enough VM at this time, this will definitely fail.
  values = (unsigned char *) mmap(0,
                        finfo.st_size,
                        PROT_READ | PROT_WRITE,
                        MAP_PRIVATE,
                        fd,
                        0);
  assert((values != MAP_FAILED) && "Trace mmap() failed!\n");

  // init fields
  currOffset = 0;
  length = finfo.st_size;

  long pages = sysconf(_SC_PHYS_PAGES);
  long page_size = sysconf(_SC_PAGE_SIZE);
  StoreValueCacheBytes = static_cast<long>(pages * LOAD_FACTOR ) * page_size;
}

void StoreValueReader::checkCacheBoundaryAndUpdateCurrOffset(uintptr_t len) {
  unsigned char* currAddr = values + currOffset;
  unsigned char* nextAddr = currAddr + len;
  if ((uintptr_t) nextAddr % StoreValueCacheBytes > 0 &&
      (uintptr_t) nextAddr / StoreValueCacheBytes >
      (uintptr_t)currAddr / StoreValueCacheBytes) {
    currAddr = nextAddr - (uintptr_t) nextAddr % StoreValueCacheBytes;
    currOffset = currAddr - values;
  }
}

void StoreValueReader::getNextValue(unsigned char *dest, uintptr_t len) {
  checkCacheBoundaryAndUpdateCurrOffset(len);

  unsigned char* currAddr = values + currOffset;
  memcpy(dest, currAddr, len);

  currOffset += len;
}

void StoreValueReader::moveCurrOffset(uintptr_t len) {
  checkCacheBoundaryAndUpdateCurrOffset(len);
  currOffset += len;
}

void StoreValueReader::close() {
  munmap(values, length);
}
