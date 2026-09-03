// Boost: GameObject Object Pool
// GD creates and destroys thousands of GameObjects during gameplay, especially
// on retry. Each create/destroy involves malloc/free + constructor/destructor.
// We provide a fixed-size pool that recycles memory blocks of the right size,
// eliminating allocation overhead on the hot path.
//
// We hook the CRT malloc/free for blocks matching GameObject size (~400-800
// bytes).

#include "angle_loader.hpp"
#include "config.hpp"
#include <atomic>
#include <cstring>
#include <mutex>
#include <vector>
#include <windows.h>


namespace {

struct Pool {
  static constexpr size_t MIN_SIZE = 256;
  static constexpr size_t MAX_SIZE = 1024;
  static constexpr size_t ALIGN = 16;

  struct Block {
    Block *next;
  };

  Block *freeList = nullptr;
  std::mutex mu;
  std::vector<void *> chunks; // big allocations backing the pool
  size_t blockSize = 0;
  size_t capacity = 0;
  std::atomic<int> hits{0};
  std::atomic<int> misses{0};

  void init(int cap, size_t bsize) {
    capacity = cap;
    blockSize = (bsize + ALIGN - 1) & ~(ALIGN - 1);
    size_t chunkSize = blockSize * capacity;
    void *mem = VirtualAlloc(nullptr, chunkSize, MEM_COMMIT | MEM_RESERVE,
                             PAGE_READWRITE);
    if (!mem)
      return;
    chunks.push_back(mem);

    auto *base = (uint8_t *)mem;
    for (int i = 0; i < capacity; i++) {
      auto *block = (Block *)(base + i * blockSize);
      block->next = freeList;
      freeList = block;
    }
  }

  void *alloc(size_t n) {
    if (n < MIN_SIZE || n > MAX_SIZE) {
      misses++;
      return nullptr;
    }

    std::lock_guard<std::mutex> lk(mu);
    if (!freeList) {
      misses++;
      return nullptr;
    }

    Block *b = freeList;
    freeList = b->next;
    hits++;
    return (void *)b;
  }

  void dealloc(void *p) {
    if (!p)
      return;

    // check if pointer belongs to our chunks
    for (auto *chunk : chunks) {
      auto *base = (uint8_t *)chunk;
      auto *ptr = (uint8_t *)p;
      if (ptr >= base && ptr < base + blockSize * capacity) {
        std::lock_guard<std::mutex> lk(mu);
        auto *block = (Block *)p;
        block->next = freeList;
        freeList = block;
        return;
      }
    }
    // not our memory - caller should use original free
  }

  bool owns(void *p) const {
    for (auto *chunk : chunks) {
      auto *base = (uint8_t *)chunk;
      auto *ptr = (uint8_t *)p;
      if (ptr >= base && ptr < base + blockSize * capacity)
        return true;
    }
    return false;
  }

  ~Pool() {
    for (auto *c : chunks)
      VirtualFree(c, 0, MEM_RELEASE);
  }
};

Pool *g_pool = nullptr;

} // namespace

namespace boost_obj_pool {

void apply() {
  auto &cfg = Config::get();
  if (!cfg.object_pool)
    return;

  int poolSize = cfg.object_pool_size;
  if (poolSize < 256)
    poolSize = 256;
  if (poolSize > 65536)
    poolSize = 65536;

  g_pool = new Pool();
  g_pool->init(poolSize, 768); // typical GameObject is ~400-700 bytes
  angle::log("obj_pool: %d slots of %zu bytes ready", poolSize,
             g_pool->blockSize);
}

// For external callers (e.g., from IAT hook on malloc)
void *tryAlloc(size_t n) { return g_pool ? g_pool->alloc(n) : nullptr; }

bool tryFree(void *p) {
  if (!g_pool || !p)
    return false;
  if (!g_pool->owns(p))
    return false;
  g_pool->dealloc(p);
  return true;
}

void shutdown() {
  if (g_pool) {
    angle::log("obj_pool: hits=%d, misses=%d", g_pool->hits.load(),
               g_pool->misses.load());
    delete g_pool;
    g_pool = nullptr;
  }
}
} // namespace boost_obj_pool
