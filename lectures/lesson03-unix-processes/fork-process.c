
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <stdbool.h>
#include <sys/wait.h>
#include <errno.h>
#include <assert.h>

#define FIXME 0

int main(int argc, char *argv[])
{
	pid_t pid;

	printf("Parent PID: %d\n", getpid());

	pid = fork();
	if (pid < 0) {
		fprintf(stderr, "Failed to fork: %s\n", strerror(errno));
		return -1;
	}
	
	if (pid == 0) {
		/* Дочерний процесс */

		exit(5);
	} else if (pid > 0) {
		/* Родительский процесс */
		int child_status;
		pid_t terminated_pid;

		terminated_pid = waitpid(pid, &child_status, 0);

		printf("Child PID: %d\n", pid);
		/* Для работы с переменной child_status используются макросы
		 * WIFEXITED, WEXITSTATUS, WIFSIGNALED, WTERMSIG.
		 */
		if (WIFEXITED(child_status))
			printf("Child %d exited with status %d\n",
			       pid, WEXITSTATUS(child_status));
		else if (WIFSIGNALED(child_status))
			printf("Child %d was terminated by signal %d\n",
			       pid, WTERMSIG(child_status));
	}
	return 0;
}
