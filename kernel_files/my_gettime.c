#include <linux/linkage.h>
#include <linux/kernel.h>
#include <linux/ktime.h>
#include <linux/timekeeping.h>
#include <linux/uaccess.h>

asmlinkage int sys_my_gettime(void* timespec) {
  struct timespec kernel_timespec;

  getnstimeofday(&kernel_timespec);
  copy_to_user(timespec, &kernel_timespec, sizeof(struct timespec));

  return 0;
}

