#include <linux/linkage.h>
#include <linux/kernel.h>

asmlinkage int sys_my_printtime(int pid, long start_sec, long start_nsec,
                                long finish_sec, long finish_nsec) {
  printk("[Project1] %d %ld.%09ld %ld.%09ld\n", pid, start_sec, start_nsec,
                                                finish_sec, finish_nsec);

  return 0;
}

