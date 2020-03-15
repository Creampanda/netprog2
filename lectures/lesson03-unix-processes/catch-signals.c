
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdbool.h>
#include <sys/signal.h>
#include <errno.h>
#include <assert.h>

#define FIXME 0

static bool sigterm;
static bool sigint;
static bool sigchld;
static bool sigkill;

bool somesignal = false;

static void signal_handler(int sig)
{
	somesignal = true;
	switch (sig)
	{
	case SIGINT:
		sigint = true;
		break;
	case SIGTERM:
		sigterm = true;
		break;
	case SIGCHLD:
		sigchld = true;
		break;
	case SIGKILL:
		sigkill = true;
		break;
	default:
		break;
	}
}

int main(int argc, char *argv[])
{
	struct sigaction sa;
	int ret;

	/* Регистрация обработчика для SIGTERM, SIGINT, SIGKILL, SIGCHLD. */

	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = signal_handler;
	sigaction(SIGINT, &sa, NULL);
	sigaction(SIGTERM, &sa, NULL);
	sigaction(SIGKILL, &sa, NULL);
	sigaction(SIGCHLD, &sa, NULL);

	/* ... */

	while (1) {
		char buf[100];

		ret = read(STDIN_FILENO, buf, sizeof(buf));
		if (somesignal) {
			printf("Interrupted by signal: "
			       "term %d, int %d, chld %d, kill %d\n",
			       sigterm, sigint, sigchld, sigkill);
		}
	}

	return 0;
}
