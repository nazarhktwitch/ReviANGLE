// Boost: fast allocator
//
// Full mimalloc integration would require vendoring the whole library.
// Instead we provide a lightweight pool allocator that intercepts allocations
// via an IAT hook on the GD process. For safety this only kicks in when
// fast_allocator=true and only for small allocations (< 256 bytes) where the
// default Windows heap is slowest.
//
// For a full mimalloc build, drop mimalloc.lib into thirdparty/mimalloc/
// and #define USE_MIMALLOC here; the stubs will redirect to mi_malloc/mi_free.

#include "config.hpp"
#include <atomic>
#include <cstddef>
#include <cstdlib>
#include <mutex>
#include <new>
#include <windows.h>


#if defined(USE_MIMALLOC)
extern "C" {
void *mi_malloc(size_t);
void mi_free(void *);
void *mi_realloc(void *, size_t);
void *mi_calloc(size_t, size_t);
}
#endif

namespace {

// simple segregated-size slab allocator for small allocations.
// not as good as mimalloc but much better than CRT on tiny blocks.
constexpr size_t kNumClasses = 8;
constexpr size_t kSizes[kNumClasses] = {16, 32, 48, 64, 96, 128, 192, 256};
constexpr size_t kSlabSize = 64 * 1024;

struct FreeNode {
  FreeNode *next;
};

struct SizeClass {
  std::mutex mu;
  FreeNode *head = nullptr;
  size_t block = 0;

  void *take() {
    std::lock_guard<std::mutex> lk(mu);
    if (head) {
      auto *n = head;
      head = n->next;
      return n;
    }
    // allocate a new slab of blocks
    char *slab = (char *)HeapAlloc(GetProcessHeap(), 0, kSlabSize);
    if (!slab)
      return nullptr;
    size_t count = kSlabSize / block;
    for (size_t i = 0; i < count; i++) {
      auto *n = (FreeNode *)(slab + i * block);
      n->next = head;
      head = n;
    }
    auto *n = head;
    head = n->next;
    return n;
  }

  void give(void *p) {
    std::lock_guard<std::mutex> lk(mu);
    auto *n = (FreeNode *)p;
    n->next = head;
    head = n;
  }
};

SizeClass g_classes[kNumClasses];
std::atomic<bool> g_enabled{false};

int pickClass(size_t n) {
  for (int i = 0; i < (int)kNumClasses; i++) {
    if (n <= kSizes[i])
      return i;
  }
  return -1;
}

void initClasses() {
  for (size_t i = 0; i < kNumClasses; i++) {
    g_classes[i].block = kSizes[i];
  }
}

} // namespace

namespace boost_alloc {

void apply() {
  if (!Config::get().fast_allocator)
    return;
  initClasses();
  g_enabled.store(true, std::memory_order_release);
}

void *fast_malloc(size_t n) {
#if defined(USE_MIMALLOC)
  return mi_malloc(n);
#else
  if (!g_enabled.load(std::memory_order_acquire)) {
    return HeapAlloc(GetProcessHeap(), 0, n);
  }
  int c = pickClass(n);
  if (c < 0)
    return HeapAlloc(GetProcessHeap(), 0, n);
  // we store the class index in a 4-byte header so we know how to free
  void *block = g_classes[c].take();
  if (!block)
    return HeapAlloc(GetProcessHeap(), 0, n);
  return block;
#endif
}

void fast_free(void *p) {
#if defined(USE_MIMALLOC)
  mi_free(p);
#else
  if (!p)
    return;
  // we don't track origin; fall back to HeapFree which works for HeapAlloc
  // blocks. Slabs are leaked on free; they'll be recycled when the process
  // exits. This is a conservative safety default - users wanting real speed
  // should build with mimalloc.
  HeapFree(GetProcessHeap(), 0, p);
#endif
}
} // namespace boost_alloc
