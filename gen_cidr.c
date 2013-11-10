#include <stdio.h>

#define LEAD_ONES(X) 0xFFFFFFFF << (32-X)
int main(int argc, char** argv) {
  int i;
  unsigned int ip, netsize;
  srand(time(0));
  for (i = 1; i <= atoi(argv[1]); i++) {
    netsize = (rand() % 31) + 1;
    ip = (rand() % (1 << netsize)) << (32-netsize);
    // ip = ip & LEAD_ONES(netsize);

    printf("T %u.%u.%u.%u/%u %d\n", (ip >> 24) & 0xFF, (ip >> 16) & 0xFF, (ip >> 8) & 0xFF, ip & 0xFF, netsize, i);
  }
  return 0;
}
