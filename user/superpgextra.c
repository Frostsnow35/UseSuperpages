// Extra lifecycle checks; the official pgtbltest remains unchanged.
#include "kernel/types.h"
#include "kernel/riscv.h"
#include "kernel/fcntl.h"
#include "user/user.h"

static void
check(int ok, char *why)
{
  if(!ok){
    printf("superpgextra: FAIL %s\n", why);
    exit(1);
  }
}

int
main(void)
{
  uint64 original = (uint64)sbrk(0);
  uint64 base = SUPERPGROUNDUP(original);
  check(sbrk(base - original) != SBRK_ERROR, "align heap");

  for(int round = 0; round < 24; round++){
    char *p = sbrk(2 * SUPERPGSIZE);
    check((uint64)p == base, "grow repeated region");
    pte_t first = pgpte(p);
    check(first != 0 && first == (pte_t)pgpte(p + PGSIZE), "first L1 leaf");
    check(pgpte(p + SUPERPGSIZE) == pgpte(p + SUPERPGSIZE + PGSIZE), "second L1 leaf");

    // Exercise copyin/copyout at a nonzero offset in a superpage and
    // across a 4 KiB boundary. Hardware loads alone cannot catch this.
    char *src = p + 2 * PGSIZE - 20;
    char *dst = p + 4 * PGSIZE - 40;
    for(int i = 0; i < 96; i++)
      src[i] = i + round;
    int fd[2];
    check(pipe(fd) == 0, "pipe");
    check(write(fd[1], src, 96) == 96, "copyin");
    check(read(fd[0], dst, 96) == 96, "copyout");
    for(int i = 0; i < 96; i++)
      check(dst[i] == src[i], "copy offset/data");
    close(fd[0]);
    close(fd[1]);
    char *path = p + 6 * PGSIZE - 2;
    strcpy(path, "spx");
    int f = open(path, O_CREATE | O_RDWR);
    check(f >= 0, "copyinstr across page");
    close(f);
    check(unlink(path) == 0, "unlink path");

    p[SUPERPGSIZE + 1] = 42;
    check(sbrk(-PGSIZE) != SBRK_ERROR, "partial free");
    check(pgpte(p + 2 * SUPERPGSIZE - PGSIZE) == 0, "freed PTE");
    check(p[SUPERPGSIZE + 1] == 42, "demotion preserves data");
    int pid = fork();
    check(pid >= 0, "fork mixed leaves");
    if(pid == 0){
      check(p[SUPERPGSIZE + 1] == 42, "child demoted data");
      check(dst[10] == src[10], "child superpage data");
      p[SUPERPGSIZE + 1] = 7;
      src[10] = 99;
      exit(0);
    }
    int status;
    check(wait(&status) == pid && status == 0, "child status");
    check(p[SUPERPGSIZE + 1] == 42 && src[10] == (char)(10 + round), "fork isolation");
    check(sbrk(-(2 * SUPERPGSIZE - PGSIZE)) != SBRK_ERROR, "release mixed region");
  }
  printf("superpgextra: reuse/copy/fork OK\n");

  // Exhaust memory to check fallback to ordinary pages and growth rollback.
  int chunks = 0;
  while(sbrk(SUPERPGSIZE) != SBRK_ERROR)
    chunks++;
  check(chunks > 16 && chunks < 64, "super pool fallback and OOM");
  uint64 top = (uint64)sbrk(0);
  check(top == base + (uint64)chunks * SUPERPGSIZE, "failed growth rollback");
  check(pgpte((void*)top) == 0, "rollback removed mapping");
  check(sbrk(-((chunks - 1) * SUPERPGSIZE)) != SBRK_ERROR, "release pressure");
  check(sbrk(-PGSIZE) != SBRK_ERROR, "demote first block");
  check(sbrk(-(SUPERPGSIZE - PGSIZE)) != SBRK_ERROR, "release first block");
  check((uint64)sbrk(0) == base, "restore heap");
  printf("superpgextra: pool/rollback OK\n");

  // Keep an intact superpage at the parent's heap end. The child
  // consumes even the last ordinary 4 KiB page before signalling us.
  char *p = sbrk(SUPERPGSIZE);
  check((uint64)p == base, "grow before OOM demotion");
  check(pgpte(p) == pgpte(p + PGSIZE), "OOM target is L1");
  p[SUPERPGSIZE - 2 * PGSIZE + 1] = 73;
  int ready[2], done[2];
  check(pipe(ready) == 0 && pipe(done) == 0, "OOM pipes");
  int pid = fork();
  check(pid >= 0, "fork before pressure");
  if(pid == 0){
    close(ready[0]);
    close(done[1]);
    while(sbrk(SUPERPGSIZE) != SBRK_ERROR)
      ;
    while(sbrk(PGSIZE) != SBRK_ERROR)
      ;
    char token = 'R';
    check(write(ready[1], &token, 1) == 1, "signal OOM");
    check(read(done[0], &token, 1) == 1, "wait for demotion");
    exit(0);
  }
  close(ready[1]);
  close(done[0]);
  char token;
  check(read(ready[0], &token, 1) == 1 && token == 'R', "pressure ready");
  check(sbrk(-PGSIZE) != SBRK_ERROR, "demotion at OOM");
  check(pgpte(p + SUPERPGSIZE - PGSIZE) == 0, "OOM freed PTE");
  check(pgpte(p) != pgpte(p + PGSIZE), "OOM demoted to L0");
  check(p[SUPERPGSIZE - 2 * PGSIZE + 1] == 73, "OOM retained data");
  check(write(done[1], &token, 1) == 1, "release child");
  int status;
  check(wait(&status) == pid && status == 0, "OOM child status");
  close(ready[0]);
  close(done[1]);
  check(sbrk(-(SUPERPGSIZE - PGSIZE)) != SBRK_ERROR, "release OOM target");
  check(sbrk(-(base - original)) != SBRK_ERROR, "restore original break");
  printf("superpgextra: OOM demotion OK\n");
  printf("superpgextra: all tests succeeded\n");
  exit(0);
}
