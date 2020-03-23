
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <ctype.h>
#include <stdbool.h>
#include <sys/wait.h>
#include <errno.h>
#include <assert.h>

#define BUF_SIZE 128
#define MAX_CHILDREN 64

bool terminate; /* true - программа получила сигнал SIGTERM или SIGINT и должна
		 * завершиться
		 */

int nchildren = 0; /* текущее количество дочерних процессов */
struct
{
	pid_t pid;
	bool valid; /* true - ячейка содержит PID дочернего процесса, false -
		     * ячейка свободна.
		     */
} children[MAX_CHILDREN];

/*
 * Функция разбивает строку @command на массив аргументов argv, где argv[0] -
 * имя команды. Для анализа строки можно использовать sscanf(). Далее функция
 * исполняет указанную команду с помощью вызова execvp().
 */
static int exec_command(char *command)
{

	if (command[strlen(command) - 1] == '\n')
		command[strlen(command) - 1] = '\0';

	char *argv[6] = {NULL}; /* Можно считать, что аргументов не больше 4,
				 * ячейка argv после последнего аргумента
				 * должна содержать NULL.
				 */
	int i = 0;
	char *token;
	if ((token = strtok(command, " ")) != NULL)
	{
		do
		{
			argv[i++] = token;
		} while ((token = strtok(NULL, " ")) != NULL);
	}

	execvp(argv[0], argv);

	return 1;
}

/*
 * Фунция создает новый процесс с помощью вызова fork(). Далее родительский
 * процесс запоминает PID нового процесса в массиве children и инкрементирует
 * nchildren, а дочерний процесс переходит в функцию exec_command().
 */
static int fork_and_exec_command(char *command)
{
	pid_t pid;
	pid = fork();
	if (pid < 0)
	{
		fprintf(stderr, "Failed to fork: %s\n", strerror(errno));
		return -1;
	}

	if (pid == 0)
	{
		/* Дочерний процесс */
		exec_command(command);
	}
	else if (pid > 0)
	{
		/* Родительский процесс */
		children[nchildren].pid = pid;
		children[nchildren].valid = true;
		++nchildren;
	}

	return 0;
}

/*
 * Если (nchildren > 0), функция определяет статус всех завершившихся процессов
 * с помощью вызова waitpid(). Для каждого процесса выводится причина
 * завершения (макросы WIFEXITED, WIFSIGNALED) и код статуса (макрос
 * WEXITSTATUS), а также очищается ячейка в массиве children.  @blocking - true
 * означает, что функция должна ждать завершения всех процессов, false -
 * функция должна вернуться немедленно, даже если ни один процесс не
 * завершился.
 */
static int reap_dead_children(bool blocking)
{
	pid_t pid;
	int child_status;
	int i;

	while (nchildren)
	{
		for (i = 0; i < MAX_CHILDREN; i++)
		{
			if (children[i].valid)
			{
				pid = waitpid(children[i].pid, &child_status, (blocking) ? 0 : WNOHANG);
				if (pid == 0)
					return 0;
				if (WIFEXITED(child_status))
				{
					printf("Child %d exited with status %d\n",
						   pid, WEXITSTATUS(child_status));
					children[i].valid = false;
					--nchildren;
				}
				else if (WIFSIGNALED(child_status))
				{
					printf("Child %d was terminated by signal %d\n",
						   pid, WTERMSIG(child_status));
					children[i].valid = false;
					--nchildren;
				}
			}
		}
	}
	return 0;
}

static void sigchld_handler(int sig)
{
	/* Обработчик ничего не делает. Мы рассчитываем на завершение
	 * вызова read() с кодом errno EINTR.
	 */
}

/* Общий обработчик для сигналов SIGTERM и SIGINT. Обработчик отправляет всем
 * дочерним процессам сигнал SIGTERM с помощью вызова kill().
 */
static void sigterm_sigint_handler(int sig)
{
	int i;

	terminate = true;
	for (i = 0; i < MAX_CHILDREN; i++)
	{
		if (children[i].valid)
			kill(children[i].pid, SIGTERM);
	}
}

static void signal_handler(int sig)
{
	switch (sig)
	{
	case SIGINT:
		sigterm_sigint_handler(sig);
		break;
	case SIGTERM:
		sigterm_sigint_handler(sig);
		break;
	case SIGCHLD:
		sigchld_handler(sig);
		break;
	default:
		break;
	}
}

int main(int argc, char *argv[])
{
	char chain[BUF_SIZE];
	struct sigaction sa;
	int ret;

	/* Фрагмент кода регистрирует обработчики сигналов SIGCHLD, SIGTERM и
	 * SIGINT с помощью вызова sigaction().
	 */

	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = signal_handler;
	sigaction(SIGINT, &sa, NULL);
	sigaction(SIGTERM, &sa, NULL);
	sigaction(SIGCHLD, &sa, NULL);


	while (1)
	{
		memset(chain, 0, sizeof(chain));	
		if (terminate)
			break;
		
		ret = read(STDIN_FILENO, chain, sizeof(chain));
		if (ret < 0 && errno == EINTR)
		{
			printf("Interrupted by signal\n");
			ret = reap_dead_children(false);
		}
		else if (ret < 0)
		{
			fprintf(stderr, "Failed to read data from terminal: %s\n",
					strerror(errno));
			goto on_error;
		}
		else if (ret)
		{
			char *cmd;
			/*
			 * Фрагмент кода разбивает строку chain на отдельные
			 * команды ('|' - разделитель) и передает их в функцию
			 * fork_and_exec_command(). Для разбиения строки можно
			 * использовать strtok().
			 */
			if ((cmd = strtok(chain, "|")) != NULL)
			{
				fork_and_exec_command(cmd);
				while ((cmd = strtok(NULL, "|")) != NULL)
				{
					fork_and_exec_command(cmd);
				}
			}
		}
	}
	reap_dead_children(true);

	return 0;
on_error:
	return 1;
}
