#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <ctype.h>
#include <stdbool.h>
#include <stdarg.h>
#include <sys/time.h>
#include <limits.h>
#include <errno.h>
#include <assert.h>

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

enum state
{
	STOP,
	ECHO,
	PAUSE,
	LATCH,
	GENERATE,
};

/* Возможные события конечного автомата: прием цифры, прием управляющего
 * символа, истечение таймаута или разрыв соединения. Цифры и управляющие
 * символы представляются в кодировке ASCII (т.е. '1', '2', 'e', 's' и т.д.)
 * Для остальных событий используется перечислимый тип enum event.
 */
enum event
{
	PERIOD_EXPIRATION = 256,
	GENERATE_EXPIRATION,
	CONNECTION_CLOSED,
};

struct repeater
{
	unsigned int index;
	int listen_sock;
	int data_sock;

	enum state state;

	/* Данные для состояния PAUSE. */
	struct timeval pause_expiration;

	/* Данные для состояния GENERATE. */
	struct timeval period_expiration;
	struct timeval generate_expiration;
	char generated_char;
	struct timeval generate_period;
};

static const char *state2str(enum state state)
{
	const char *strings[] = {
	    [STOP] = "stop",
	    [ECHO] = "echo",
	    [PAUSE] = "pause",
	    [LATCH] = "latch",
	    [GENERATE] = "generate",
	};
	return strings[state];
}

static const char *event2str(int event)
{
	static char str[2];

	switch (event)
	{
	case PERIOD_EXPIRATION:
		return "period-expiration";
	case GENERATE_EXPIRATION:
		return "generate-expiration";
	case CONNECTION_CLOSED:
		return "connection-closed";
	default:
		break;
	}
	if (!isalnum(event))
		return NULL;
	sprintf(str, "%c", event);
	return str;
}

static int init_repeater(struct repeater *r, unsigned int index)
{
	struct sockaddr_in addr = {
	    .sin_family = AF_INET,
	    .sin_port = htons(9000 + r->index),
	    .sin_addr = {INADDR_ANY},
	};
	int option = 1;
	int ret;

	r->listen_sock = socket(AF_INET, SOCK_STREAM, 0);
	if (!r->listen_sock)
	{
		fprintf(stderr, "Failed to create socket: %s\n", strerror(errno));
		return -1;
	}

	ret = setsockopt(r->listen_sock, SOL_SOCKET, SO_REUSEADDR,
			 &option, sizeof(option));
	if (ret < 0)
	{
		fprintf(stderr, "Failed to set socket option: %s\n", strerror(errno));
		goto on_error;
	}

	ret = bind(r->listen_sock, (struct sockaddr *)&addr, sizeof(addr));
	if (ret < 0)
	{
		fprintf(stderr, "Failed to bind to server: %s\n", strerror(errno));
		goto on_error;
	}

	ret = listen(r->listen_sock, 1);
	if (ret < 0)
	{
		fprintf(stderr, "Failed to put socket into listening state: %s\n",
			strerror(errno));
		goto on_error;
	}
	r->generate_period.tv_sec = 1;
	r->state = STOP;
	return 0;
on_error:
	close(r->listen_sock);
	r->listen_sock = -1;
	return -1;
}

/* Функция форматирует сообщение с помощью vsnprintf() и записывает его в
 * передающий сокет повторителя с помощью send().
 * int vsnprintf( char *s, size_t n, const char *format, va_list ap );
 * Запись форматированной строки в строку с ограничением на количество выводимых символов. 
 * Значения для вывода передаются в функцию в виде списка va list
 */
static int send_data(struct repeater *r, const char *fmt, ...)
{
	va_list ap;
	char response[100];
	va_start(ap, fmt);

	vsnprintf(response, sizeof(response), fmt, ap);

	int ret = send(r->data_sock, response, strlen(response), 0);

	if (ret < 0)
		return -1;
	return 0;
}

static void run_state_machine(struct repeater *r, int event)
{
	enum state old_state = r->state;
	const char *event_str = event2str(event);
	struct timeval now;
	int ret;

	if (!event_str)
		return;

	printf("Received event '%s' in state %s\n",
	       event_str, state2str(r->state));

	gettimeofday(&now, NULL);

	switch (r->state)
	{
	case STOP:
		if (event == 'e')
			r->state = ECHO;
		else if (event == 'l')
			r->state = LATCH;
		break;
		/* Фрагмент кода отвечает за обработку событий в состояниях ECHO и
	 * PAUSE. Чтобы отличать цифры, от управляющих символов, можно
	 * использовать функцию isdigit(). Для работы со временем использовать
	 * макросы timeradd(), timersub(), timercmp(). Вывод повторяемых цифр
	 * или времени до истечения задержки производится с помощью
	 * send_data().
	 */

	case ECHO:
		if (isdigit(event))
		{
			ret = send_data(r, "Echoing digit '%c'\n", event);
			if (ret < 0)
				goto on_error;
		}
		else if (event == 's')
		{
			r->state = STOP;
		}
		else if (event == 'p')
		{
			struct timeval pause = {.tv_sec = 3};

			timeradd(&now, &pause, &r->pause_expiration);
			r->state = PAUSE;
		}
		break;
	case PAUSE:
		if (isdigit(event))
		{
			struct timeval left;

			if (timercmp(&now, &r->pause_expiration, >))
			{
				r->state = ECHO;
			}
			else
			{
				timersub(&r->pause_expiration, &now, &left);

				ret = send_data(r, "%ld seconds left\n", left.tv_sec);
				if (ret < 0)
					goto on_error;
			}
		}
		break;
	case LATCH:
		if (event == 's')
			r->state = STOP;
		else if (isdigit(event))
		{
			struct timeval generate = {.tv_sec = 5};

			timeradd(&now, &generate, &r->generate_expiration);
			timeradd(&now, &r->generate_period, &r->period_expiration);

			r->generated_char = event;
			r->state = GENERATE;
		}

		break;
	case GENERATE:
		if (timercmp(&now, &r->generate_expiration, >))
		{
			r->state = LATCH;
		}
		else if (timercmp(&now, &r->period_expiration, >=))
		{
			struct timeval period = {.tv_sec = 1};
			timeradd(&now, &period, &r->period_expiration);
			ret = send_data(r, "%c\n", r->generated_char);
			if (ret < 0)
				goto on_error;
		}

		break;
	}

	if (r->state != old_state)
		printf("Transitioning to state %s\n", state2str(r->state));
	return;
on_error:
	return;
}

int main(int argc, char *argv[])
{
	struct repeater repeaters[10];
	int ret;
	int i;

	memset(repeaters, 0, sizeof(repeaters));
	for (i = 0; i < ARRAY_SIZE(repeaters); i++)
	{
		repeaters[i].index = i;
		repeaters[i].listen_sock = -1;
		repeaters[i].data_sock = -1;
	}
	for (i = 0; i < ARRAY_SIZE(repeaters); i++)
	{
		ret = init_repeater(&repeaters[i], i);
		if (ret < 0)
			goto on_error;
	}

	while (1)
	{
		fd_set fds;
		int max_fd = 0; /* Наибольший файловый дескриптор, находящийся
				 * в множестве fds.
				 */
		struct timeval now,
		    timeout = {.tv_sec = LONG_MAX},
		    candidate;

		FD_ZERO(&fds);

		/* Фрагмент кода переьирает все повторители и добавляет
		 * необходимый файловый дескриптор в множество fds: дескриптор
		 * слушающего сокета, если соединение еще не установлено,
		 * дескриптор передающего сокета - если установлено.
		 */

		for (i = 0; i < ARRAY_SIZE(repeaters); ++i)
		{
			struct repeater *r = &repeaters[i];
			int sock = r->data_sock < 0 ? r->listen_sock : r->data_sock;

			FD_SET(sock, &fds);
			max_fd = (sock > max_fd) ? sock + 1 : max_fd;
		}

		/* Фрагмент кода задает значение timeout - разницу между
		 * текущим временем и самым ранним временем истечения для всех
		 * активных таймеров. Используется функция gettimeofday() и
		 * макросы timeradd(), timersub(), timercmp(). Следует учесть,
		 * что таймеры активны только у повторителей в состоянии
		 * GENERATE. Если активных таймеров нет, timeout остается без
		 * изменений.
		 */

		/* ... УСЛОЖНЕННАЯ ЗАДАЧА ... */
		gettimeofday(&now, NULL);

		for (int i = 0; i < 10; i++)
		{
			if (repeaters[i].state == GENERATE)
			{
				timersub(&repeaters[i].period_expiration, &now, &candidate);
				if (timercmp(&candidate, &timeout, <))
				{
					timeout = candidate;
				}
			}
		}

		ret = select(max_fd + 1, &fds, NULL, NULL, &timeout);
		if (ret < 0)
		{
			fprintf(stderr, "select() failed: %s\n", strerror(errno));
			goto on_error;
		}

		/* Фрагмент кода проверяет все активные таймеры и генерирует
		 * события GENERATE_EXPIRATION или PERIOD_EXPIRATION для
		 * сработавших таймеров. Используется функция gettimeofday() и
		 * макросы timeradd(), timersub(), timercmp().
		 */

		if (ret == 0)
		{
			gettimeofday(&now, NULL);
			for (int i = 0; i < 10; i++)
			{
				if (repeaters[i].state == GENERATE)
				{
					if(timercmp(&now,&repeaters[i].generate_expiration, >=))
						run_state_machine(&repeaters[i],GENERATE_EXPIRATION);
					else if (timercmp(&now,&repeaters[i].period_expiration, >=))
						run_state_machine(&repeaters[i],PERIOD_EXPIRATION);
				}
			}
		}
		/* Фрагмент кода перебирает все повторители и проверяет
		 * готовность их файловых дескрипторах. Если готов к чтению
		 * слушающий сокет, программа принимает новое передающее
		 * соединение, если готов передающий сокет, программа читает из
		 * него данные и передает их в качестве событий на вход
		 * конечного автомата. Если передающее соединение завершилось,
		 * программа отправляет автомату событие CONNECTION_CLOSED.
		 */

		for (i = 0; i < ARRAY_SIZE(repeaters); i++)
		{
			struct repeater *r = &repeaters[i];

			if (FD_ISSET(r->listen_sock, &fds))
			{
				r->data_sock = accept(r->listen_sock, NULL, NULL);
				if (r->data_sock < 0)
				{
					fprintf(stderr, "Failed to accept data "
							"connection: %s\n",
						strerror(errno));
					goto on_error;
				}
			}
			else if (r->data_sock > 0 && FD_ISSET(r->data_sock, &fds))
			{
				char ch;
				ret = recv(r->data_sock, &ch, 1, MSG_DONTWAIT);
				if (ret < 0)
					goto on_error;
				if (ret == 0)
				{
					close(r->data_sock);
					continue;
				}

				run_state_machine(r, ch);
			}
		}
	}

	for (i = 0; i < ARRAY_SIZE(repeaters); i++)
	{
		if (repeaters[i].listen_sock >= 0)
			close(repeaters[i].listen_sock);
		if (repeaters[i].data_sock >= 0)
			close(repeaters[i].data_sock);
	}
	return 0;
on_error:
	for (i = 0; i < ARRAY_SIZE(repeaters); i++)
	{
		if (repeaters[i].listen_sock >= 0)
			close(repeaters[i].listen_sock);
		if (repeaters[i].data_sock >= 0)
			close(repeaters[i].data_sock);
	}
	return 1;
}
