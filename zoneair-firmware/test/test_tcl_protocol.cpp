#include <cstdio>
#include <cstring>
#include "../src/protocol/tcl.h"
#include "fixtures/tcl_frames.h"

using namespace zoneair;

#define EXPECT(cond) do { if (!(cond)) { \
  std::fprintf(stderr, "FAIL %s:%d %s\n", __FILE__, __LINE__, #cond); \
  return 1; } } while(0)

static int test_buildQuery_matches_fixture() {
  uint8_t buf[64];
  size_t n = TclProtocol::buildQuery(buf, sizeof(buf));
  EXPECT(n == fixtures::QUERY_LEN);
  EXPECT(std::memcmp(buf, fixtures::QUERY, n) == 0);
  return 0;
}

int main() {
  if (test_buildQuery_matches_fixture()) return 1;
  std::printf("OK\n");
  return 0;
}
