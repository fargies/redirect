/*
 * Copyright (C) 2011 by Nelson Elhage
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */
#include <sys/types.h>
#include <dirent.h>
#include <sys/syscall.h>
#include <sys/mman.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <signal.h>
#include <limits.h>
#include <time.h>
#include <sys/time.h>
#include <sys/stat.h>

#include "ptrace.h"
#include "redirect.h"

#define TASK_COMM_LENGTH 16
struct proc_stat {
    pid_t pid;
    char comm[TASK_COMM_LENGTH+1];
    char state;
    pid_t ppid, sid, pgid;
    dev_t ctty;
};

#define do_syscall(child, name, a0, a1, a2, a3, a4, a5) \
    ptrace_remote_syscall((child), ptrace_syscall_numbers((child))->nr_##name, \
                          a0, a1, a2, a3, a4, a5)

int parse_proc_stat(int statfd, struct proc_stat *out) {
    char buf[1024];
    int n;
    unsigned dev;
    lseek(statfd, 0, SEEK_SET);
    if (read(statfd, buf, sizeof buf) < 0)
        return errno;
    n = sscanf(buf, "%d (%16[^)]) %c %d %d %d %u",
               &out->pid, out->comm,
               &out->state, &out->ppid, &out->sid,
               &out->pgid, &dev);
    if (n == EOF)
        return errno;
    if (n != 7) {
        return EINVAL;
    }
    out->ctty = dev;
    return 0;
}

static void do_unmap(struct ptrace_child *child, child_addr_t addr, unsigned long len) {
    if (addr == (unsigned long)-1)
        return;
    do_syscall(child, munmap, addr, len, 0, 0, 0, 0);
}

/*
 * Wait for the specific pid to enter state 'T', or stopped. We have to pull the
 * /proc file rather than attaching with ptrace() and doing a wait() because
 * half the point of this exercise is for the process's real parent (the shell)
 * to see the TSTP.
 *
 * In case the process is masking or ignoring SIGTSTP, we time out after a
 * second and continue with the attach -- it'll still work mostly right, you
 * just won't get the old shell back.
 */
void wait_for_stop(pid_t pid, int fd) {
    struct timeval start, now;
    struct timespec sleep;
    struct proc_stat st;

    gettimeofday(&start, NULL);
    while (1) {
        gettimeofday(&now, NULL);
        if ((now.tv_sec > start.tv_sec && now.tv_usec > start.tv_usec)
            || (now.tv_sec - start.tv_sec > 1)) {
            error("Timed out waiting for child stop.");
            break;
        }
        /*
         * If anything goes wrong reading or parsing the stat node, just give
         * up.
         */
        if (parse_proc_stat(fd, &st))
            break;
        if (st.state == 'T')
            break;

        sleep.tv_sec  = 0;
        sleep.tv_nsec = 10000000;
        nanosleep(&sleep, NULL);
    }
}

int redirect_child(pid_t pid, const char *log_file) {
    struct ptrace_child child;
    unsigned long scratch_page = -1;
    int statfd, child_fd;
    int err = 0;
    long page_size = sysconf(_SC_PAGE_SIZE);
    char stat_path[PATH_MAX];
    long mmap_syscall;

    snprintf(stat_path, sizeof stat_path, "/proc/%d/stat", pid);
    statfd = open(stat_path, O_RDONLY);
    if (statfd < 0) {
        error("Unable to open %s: %s", stat_path, strerror(errno));
        return -statfd;
    }

    kill(pid, SIGTSTP);
    wait_for_stop(pid, statfd);

    if (ptrace_attach_child(&child, pid)) {
        err = child.error;
        goto out_cont;
    }

    if (ptrace_advance_to_state(&child, ptrace_at_syscall)) {
        err = child.error;
        goto out_detach;
    }
    if (ptrace_save_regs(&child)) {
        err = child.error;
        goto out_detach;
    }

    mmap_syscall = ptrace_syscall_numbers(&child)->nr_mmap2;
    if (mmap_syscall == -1)
        mmap_syscall = ptrace_syscall_numbers(&child)->nr_mmap;
    scratch_page = ptrace_remote_syscall(&child, mmap_syscall, 0,
                                         page_size, PROT_READ|PROT_WRITE,
                                         MAP_ANONYMOUS|MAP_PRIVATE, 0, 0);

    if (scratch_page > (unsigned long)-1000) {
        err = -(signed long)scratch_page;
        goto out_unmap;
    }

    debug("Allocated scratch page: %lx", scratch_page);

    if (ptrace_memcpy_to_child(&child, scratch_page, log_file, strlen(log_file)
                + 1)) {
        err = child.error;
        error("Unable to memcpy the log file path to child.");
        goto out_unmap;
    }

    child_fd = do_syscall(&child, open,
                          scratch_page, O_WRONLY|O_APPEND|O_CREAT,
                          0777, 0, 0, 0);
    if (child_fd < 0) {
        err = child_fd;
        error("Unable to open the log file in the child.");
        goto out_unmap;
    }

    debug("Opened the new log file in the child: %d", child_fd);

    if (do_syscall(&child, dup2, child_fd, 1, 0, 0, 0, 0) != 1)
    {
        err = 1;
        error("Unable to dup2 stdout in the child.");
        goto out_close;
    }

    if (do_syscall(&child, dup2, child_fd, 2, 0, 0, 0, 0) != 2)
    {
        err = 1;
        error("Unable to dup2 stderr in the child.");
        goto out_close;
    }

 out_close:
    do_syscall(&child, close, child_fd, 0, 0, 0, 0, 0);

 out_unmap:
    do_unmap(&child, scratch_page, page_size);

    ptrace_restore_regs(&child);
 out_detach:
    ptrace_detach_child(&child);

    if (err == 0) {
        kill(child.pid, SIGSTOP);
        wait_for_stop(child.pid, statfd);
    }
 out_cont:
    kill(child.pid, SIGCONT);
    close(statfd);

    return err < 0 ? -err : err;
}

