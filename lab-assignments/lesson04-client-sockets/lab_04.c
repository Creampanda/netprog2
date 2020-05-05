
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
#include <errno.h>
#include <assert.h>

#define BUF_SIZE 128

typedef int ftp_code_t; /* -1 - ошибка, >= 0 - код от FTP-сервера */

/* Функция создает потоковый сетевой сокет и устанавливает соединение с
 * локальным сервером (IP-адрес 127.0.0.1) на указанном порте @port. Далее
 * функция открывает поток ввода-вывода на этом сокете и отдает его в качестве
 * возвращаемого значения. Функция использует библиотечные вызовы socket(),
 * connect(), fdopen() и inet_aton().
 */
static FILE *ftp_connect(unsigned short port)
{
	int sock = -1;
	struct sockaddr_in addr;
	int ret;

	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	inet_aton("127.0.0.1", &addr.sin_addr);

	sock = socket(AF_INET, SOCK_STREAM, 0);

	if (sock < 0)
	{
		fprintf(stderr, "Failed to create TCP socket: %s\n", strerror(errno));
		return NULL;
	}

	ret = connect(sock, (struct sockaddr *)&addr, sizeof(addr));

	if (ret < 0)
		return NULL;

	return fdopen(sock, "r+");
	;
}

/* Функция формирует FTP-запрос из аргументов @ftp_cmd и @ftp_arg и передает
 * его через управляющее соединение @control_stream в FTP-сервер. После этого
 * функция читает и анализирует ответ от сервера. Код статуса из ответа
 * используется как возвращаемое значение, остальной текст ответа помещается в
 * строку @ftp_reply_text.
 * Аргументы @ftp_cmd и @ftp_arg являются опциональными. Если @ftp_cmd равен
 * NULL, функция сразу ждет ответ от сервера.
 * Функция использует вызовы fprintf(), fgets(), sscanf() и fflush().
 */
static ftp_code_t ftp_command(FILE *control_stream,
			      const char *ftp_cmd,
			      const char *ftp_arg,
			      char *ftp_reply_text)
{
	ftp_code_t ftp_ret;
	int ret;
	char rx_buf[BUF_SIZE];
	int len;

	//printf("sfgasfas");
	// Сразу ждем ответ от сервера
	if (ftp_cmd != NULL)
	{
		//printf("%s %s %s", ftp_cmd, ftp_arg, ftp_reply_text);
		if (ftp_arg == NULL)
			ret = fprintf(control_stream, "%s\r\n", ftp_cmd);
		else
			ret = fprintf(control_stream, "%s %s\r\n", ftp_cmd, ftp_arg);
		if (ret < 0)
			return -1;
		fflush(control_stream);
	}

	// Получаем строку из control_stream в rx_buf
	if (fgets(rx_buf, sizeof(rx_buf), control_stream) == NULL)
	{
		printf("Server closed connection\n");
		return -1;
	}
	// Считываем данные из rx_buf в ftp_ret
	ret = sscanf(rx_buf, "%d %n", &ftp_ret, &len);
	if (ret < 0)
	{
		fprintf(stderr, "Invalid response '%s'\n", rx_buf);
		return -1;
	}
	// Копируем ответ сервера (без кода)
	strcpy(ftp_reply_text, rx_buf + len);

	return ftp_ret;
}

static int ftp_login(FILE *control_stream, const char *user, const char *password)
{
	ftp_code_t ftp_ret;
	char ftp_reply_text[BUF_SIZE];

	ftp_ret = ftp_command(control_stream, "USER", user, ftp_reply_text);
	if (ftp_ret < 0)
	{
		return -1;
	}
	else if (ftp_ret != 331)
	{
		fprintf(stderr, "Invalid user name: %s\n", ftp_reply_text);
		return 0;
	}

	ftp_ret = ftp_command(control_stream, "PASS", password, ftp_reply_text);
	if (ftp_ret < 0)
	{
		return -1;
	}
	else if (ftp_ret != 230)
	{
		fprintf(stderr, "Invalid password: %s\n", ftp_reply_text);
		return 0;
	}
	return 0;
}

/* Функция устанавливает дополнительное соединение для передачи данных от
 * FTP-сервера. Для этого отправляется команда PASV. Из ответа от сервера
 * извлекается номер порта, на котором сервер ждет подключения. После установки
 * соединения функция отправляет серверу команду RETR с аргументом @file_name и
 * читает данные файла из дополнительного соединения. Полученные данные
 * записываются в файл с таким же именем в текущем каталоге.
 */
static int ftp_retrieve(FILE *control_stream, const char *file_name)
{

	/* ... УСЛОЖНЕННАЯ ЗАДАЧА ... */

	return 0;
}

int main(int argc, char *argv[])
{
	FILE *control_stream = NULL;
	int ret;

	//Устанавливаем соединение
	control_stream = ftp_connect(21);

	//Если вернулся 0
	if (!control_stream)
	{
		fprintf(stderr, "Failed to create control stream\r\n");
		return 1;
	}
	char reply_txt[BUF_SIZE];
	//Делаем пустой запрос и ждем ответ
	ret = ftp_command(control_stream, NULL, NULL, reply_txt);

	if (ret < 0)
		goto on_error;

	printf("Code: %d. Reply text: %s", ret, reply_txt);

	if (argc > 1)
	{
		ret = ftp_login(control_stream, argv[1], argc > 2 ? argv[2] : NULL);
		if (ret < 0)
		{
			fprintf(stderr, "Failed to log in to FTP server\r\n");
			goto on_error;
		}
		printf("Logged in as %s\n", argv[1]);
	}

	while (1)
	{
		char *ftp_cmd = (char *)malloc(sizeof(char) * BUF_SIZE),
		     *ftp_arg = (char *)malloc(sizeof(char) * BUF_SIZE);
		;
		char command[BUF_SIZE];
		char ftp_reply_text[BUF_SIZE];
		ftp_code_t ftp_ret;

		int len;
		/* Фрагмент кода читает и анализирует команду пользователя с
		 * помощью fgets() и sscanf(). Далее если введена команда RETR,
		 * вызывается функция ftp_retrieve(). В остальных случаях -
		 * ftp_command(). По результатам выполнения выводится код
		 * статуса и дополнительное сообщение от сервера.
		 */

		/* (FILE *control_stream,
			      const char *ftp_cmd,
			      const char *ftp_arg,
			      char *ftp_reply_text) */

		//scanf("%s", command);
		fgets(command, BUF_SIZE, stdin);

		fflush(stdin);
		sscanf(command, "%s %n", ftp_cmd, &len);
		ftp_ret = ftp_command(control_stream,
				      ftp_cmd,
				      command + len,
				      ftp_reply_text);
		if (ftp_ret < 0)
		{
			goto on_error;
		}

		printf("Code: %d. Reply text: %s", ftp_ret, ftp_reply_text);
	}

	return 0;
on_error:
	fclose(control_stream);
	return 1;
}
