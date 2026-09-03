// Boost: Vertex Deduplication
// GD sends 6 vertices per quad (2 triangles, no indexing). Adjacent quads
// share 2 vertices. We detect shared vertices in glBufferData and build
// a deduped vertex + index buffer, saving ~33% of vertex bandwidth.

#include "angle_loader.hpp"
#include "config.hpp"
#include "gl_proxy.hpp"
#include <cstring>
#include <unordered_map>
#include <vector>
#include <windows.h>


typedef unsigned int GLenum;
typedef int GLsizei;
typedef unsigned short GLushort;

static bool g_active = false;
static int g_dedupSaved = 0;

namespace {

// Hash a vertex (assuming 20 bytes: float3 pos + float2 uv)
struct VertexHash {
  size_t operator()(uint64_t key) const {
    return (size_t)(key * 0x9E3779B97F4A7C15ULL >> 32);
  }
};

uint64_t hashVertex(const uint8_t *v, int stride) {
  uint64_t h = 14695981039346656037ULL;
  for (int i = 0; i < stride && i < 32; i++) {
    h ^= v[i];
    h *= 1099511628211ULL;
  }
  return h;
}

} // namespace

namespace boost_vertex_dedup {
void apply() {
  if (!Config::get().vertex_dedup)
    return;
  g_active = true;
  angle::log("vertex_dedup: active - will deduplicate shared quad vertices");
}

// Deduplicate vertices in a buffer. Returns index count.
int dedup(const void *srcVerts, int vertCount, int stride, void *outVerts,
          int *outVertCount, GLushort *outIndices) {
  if (!g_active || !srcVerts || vertCount < 6)
    return 0;

  const auto *src = (const uint8_t *)srcVerts;
  auto *dst = (uint8_t *)outVerts;
  std::unordered_map<uint64_t, GLushort, VertexHash> seen;
  seen.reserve(vertCount);

  int uniqueCount = 0;
  int indexCount = 0;

  for (int i = 0; i < vertCount; i++) {
    uint64_t h = hashVertex(src + i * stride, stride);
    auto it = seen.find(h);
    if (it != seen.end()) {
      outIndices[indexCount++] = it->second;
      g_dedupSaved++;
    } else {
      GLushort idx = (GLushort)uniqueCount;
      seen[h] = idx;
      std::memcpy(dst + uniqueCount * stride, src + i * stride, stride);
      outIndices[indexCount++] = idx;
      uniqueCount++;
    }
  }

  *outVertCount = uniqueCount;
  return indexCount;
}

bool isActive() { return g_active; }
int getSavedCount() { return g_dedupSaved; }
} // namespace boost_vertex_dedup
