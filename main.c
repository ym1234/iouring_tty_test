#include <sys/wait.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <stddef.h>
#include <sys/types.h>
#include <pwd.h>
#include <pty.h>
#include <signal.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <fcntl.h>
/* #include <errno.h> */

static inline int my_read(int fd, void *buf, size_t nbytes) {
	ssize_t ret;
	asm volatile
		(
		 "syscall"
		 : "=a" (ret)
		 //RAX             RDI      RSI       RDX
		 : "0"(__NR_read), "D"(fd), "S"(buf), "d"(nbytes)
		 : "rcx", "r11", "memory"
		);
	return ret;
}

#define die(...)
#define DEFAULT(a, b)		(a) = (a) ? (a) : (b)
/* void die(...) { }; */
static pid_t pid;
void execsh(char *cmd, char **args) {
	setbuf(stdout, NULL);
	char *sh, *prog, *arg;
	const struct passwd *pw;

	errno = 0;
	if ((pw = getpwuid(getuid())) == NULL) {
		if (errno)
			die("getpwuid: %s\n", strerror(errno));
		else
			die("who are you?\n");
	}

	if ((sh = getenv("SHELL")) == NULL)
		sh = (pw->pw_shell[0]) ? pw->pw_shell : cmd;

	if (args) {
		prog = args[0];
		arg = NULL;
	} else {
		prog = sh;
		arg = NULL;
	}

	DEFAULT(args, ((char *[]) {prog, arg, NULL}));

	unsetenv("COLUMNS");
	unsetenv("LINES");
	unsetenv("TERMCAP");
	setenv("LOGNAME", pw->pw_name, 1);
	setenv("USER", pw->pw_name, 1);
	setenv("SHELL", sh, 1);
	setenv("HOME", pw->pw_dir, 1);
	setenv("TERM", "HELLO", 1);

	signal(SIGCHLD, SIG_DFL);
	signal(SIGHUP, SIG_DFL);
	signal(SIGINT, SIG_DFL);
	signal(SIGQUIT, SIG_DFL);
	signal(SIGTERM, SIG_DFL);
	signal(SIGALRM, SIG_DFL);
	/* printf("prog: %s, args:", prog); */
	/* for (int i = 0; args[i] != NULL; i++) { */
	/* 	printf("%s\n", args[i]); */
	/* } */
	execvp(prog, args);
	_exit(1);
}


static struct termios orig_termios;

void disableRawMode() {
  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios) == -1) die("tcsetattr"); }

void enableRawMode() {
	if (tcgetattr(STDIN_FILENO, &orig_termios) == -1) die("tcgetattr");
	atexit(disableRawMode);
	struct termios raw = orig_termios;
	raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
	raw.c_oflag &= ~(OPOST);
	/* raw.c_cflag |= (CS8); */
	raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
	raw.c_lflag &= ~(ICANON | IEXTEN | ISIG);
	raw.c_cc[VMIN] = 1;
	raw.c_cc[VTIME] = 0;
	if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) die("tcsetattr");
}

// Always runs the command with the user's shell
int ttynew(char *cmd, char **args) {
	int m, s;

	if (openpty(&m, &s, NULL, NULL, NULL) < 0)
		die("openpty failed: %s\n", strerror(errno));

	switch (pid = fork()) {
	case -1:
		die("fork failed: %s\n", strerror(errno));
		break;
	case 0:
		setsid(); /* create a new process group */
		dup2(s, 0);
		dup2(s, 1);
		dup2(s, 2);
		if (ioctl(s, TIOCSCTTY, NULL) < 0)
			die("ioctl TIOCSCTTY failed: %s\n", strerror(errno));

		close(s);
		close(m);
		enableRawMode();
		execsh(cmd, args);
		break;
	default:
		close(s);
		/* signal(SIGCHLD, sigchld); */
		signal(SIGCHLD, SIG_DFL);
		break;
	}
	return m;
}

int main(int argc, char *argv[]) {
#define SIZE 4096
  char x[SIZE] = {};
	int fd = ttynew("/bin/bash", argv + 1);
	// Doesn't really help
  /* int flags = fcntl(fd, F_GETFL, 0); */
  /* fcntl(fd, F_SETFL, flags | O_NONBLOCK); */
	int z = 0;
  while (__builtin_expect(z > -1, 1)) {
		/* z = read(fd, x, SIZE); */
		z = my_read(fd, x, SIZE);
		/* x[z-1] = 0; */
		/* printf("read: %d\n", z); */
		/* printf("%s\n", errstr(errno)); */
	}
}
