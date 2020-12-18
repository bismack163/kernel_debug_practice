/*
 * trace c program all functions and system calls
 * usage: add this file to build, add CC flag -finstrument-functions run program with strace
 * https://balau82.wordpress.com/2010/10/06/trace-and-profile-function-calls-with-gcc/
 *
 *
 */
#include <stdio.h>
#include <time.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>

pid_t getpid(void);

static FILE *fp_trace;
int fd;

void
__attribute__ ((constructor))
trace_begin(void) {
// fp_trace = fopen("trace.out", "w");
//	fd = open("trace.out", O_WRONLY|O_CREAT);
	fd = open("/dev/null", O_WRONLY);
	printf("========== fd=%d\n", fd);
}

void
__attribute__ ((destructor))
trace_end(void) {
	if (fd >= 0) {
		close(fd);
	}
// if(fp_trace != NULL) {
// fclose(fp_trace);
// }
}

void
__attribute__((no_instrument_function))
__cyg_profile_func_enter(void *func, void *caller) {
	static char buf[200];
	int cnt;
	if (fd > 0) {
		cnt = snprintf(buf, 199, "==== e %p %p \n", func, caller);
		write(fd, buf, cnt);
	}
}

void
__attribute__((no_instrument_function))
__cyg_profile_func_exit(void *func, void *caller) {
	static char buf[200];
	int cnt;
	if (fd > 0) {
		cnt = snprintf(buf, 199, "==== x %p %p \n", func, caller);
		write(fd, buf, cnt);
	}
}

