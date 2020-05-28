
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

#include <picohttpparser.h>

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))
#define MESSAGE_SIZE 1024
#define METHOD_SIZE 16
#define PATH_SIZE 32
#define BODY_SIZE 64

enum http_status
{
        HTTP_OK = 200,
        HTTP_NOT_FOUND = 404,
        HTTP_METHOD_NOT_ALLOWED = 405,
        HTTP_INTERNAL_SERVER_ERROR = 501,
};

/* Структура описывает динамические строки (ресурсы http://dynamic/<name>).
 * next и prev используются для включения объекта в связный список.
 */
struct http_string
{
        struct http_string *next;
        struct http_string *prev;
        char name[PATH_SIZE];
        char value[BODY_SIZE];
};

static const char *status2str(enum http_status status)
{
        const char *strings[] = {
            [HTTP_OK] = "Ok",
            [HTTP_NOT_FOUND] = "Not found",
            [HTTP_METHOD_NOT_ALLOWED] = "Method not allowed",
            [HTTP_INTERNAL_SERVER_ERROR] = "Internal server error",
        };
        return strings[status];
}

/* Функция исполняет запрос к статическим строкам. Поддерживается только метод
 * GET и два имени строки - firstname и lastname. Функция записывает
 * предопределенное значение строки (имя и фамилия студента) в строку
 * @resp_body. Если метод или имя не поддерживаются, возвращается
 * соответствующий код.
 */
static enum http_status handle_static(const char *method,
                                      const char *name,
                                      char resp_body[])
{
        char firstname[] = "John";
        char lastname[] = "Connor";
        if (strcmp(method, "GET"))
                return HTTP_METHOD_NOT_ALLOWED;
        if (strcmp(name, firstname))
                return HTTP_NOT_FOUND;
        memcpy(resp_body, firstname, strlen(firstname));
        //strcat(resp_body, lastname);
        resp_body[5] = '\0';

        
        return HTTP_OK;
}

/* Функция исполняет запрос к динамическим строкам. Поддерживаются методы GET,
 * PUT и DELETE. Имена и значения строк произвольные. Текущие строки хранятся в
 * связном списке @list_head@ в виде объектов struct http_string. Для новых
 * строк память выделяется динамически.
 */
static enum http_status handle_dynamic(struct http_string *list_head,
                                       const char *method,
                                       const char *name,
                                       const char *req_body,
                                       char resp_body[])
{

        /* ... УСЛОЖНЕННАЯ ЗАДАЧА ... */

        return HTTP_OK;
}

/* Функция создает потоковый сетевой сокет, привязывает его ко всем системным
 * адресам (специальное значение INADDR_ANY и порту @port, после чего переводит
 * сокет в слушающий режим. Используются системные вызовы socket(), bind(),
 * listen().
 */
static int create_listening_socket(unsigned short port)
{
        int listen_sock = -1;
        int option = 1;

        struct sockaddr_in addr = {
            .sin_family = AF_INET,
            .sin_port = htons(port),
            .sin_addr = {INADDR_ANY},
        };
        int ret;

        listen_sock = socket(AF_INET, SOCK_STREAM, 0);
        if (listen_sock < 0)
        {
                fprintf(stderr, "Failed to create socket: %s\n", strerror(errno));
                return -1;
        }

        /* Установка опции SO_REUSEADDR позволяет создавать и привязывать сокет
	 * сразу после закрытия предыдущего сокета на этом порте. Ускоряет перезапуск
	 * программы при отладке.
	 */
        ret = setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, &option, sizeof(option));
        if (ret < 0)
        {
                fprintf(stderr, "Failed to set socket option: %s\n", strerror(errno));
                goto on_error;
        }

        ret = bind(listen_sock, (struct sockaddr *)&addr, sizeof(addr));
        if (ret < 0)
        {
                fprintf(stderr, "Failed to bind to server: %s\n", strerror(errno));
                goto on_error;
        }

        ret = listen(listen_sock, 5);
        if (ret < 0)
        {
                fprintf(stderr, "Failed to put socket into listening state: %s\n",
                        strerror(errno));
                goto on_error;
        }

        return listen_sock;
on_error:
        close(listen_sock);
        return -1;
}

static int parse_http_request(const char *buf, size_t len,
                              char method[], char path[], char body[])
{
        const char *method_ptr, *path_ptr;
        struct phr_header headers[100];
        size_t method_len, path_len, num_headers;
        int minor_version;
        int headers_len;

        num_headers = ARRAY_SIZE(headers);
        headers_len = phr_parse_request(buf, len, &method_ptr, &method_len,
                                        &path_ptr, &path_len,
                                        &minor_version, headers, &num_headers, 0);

        if (headers_len <= 0)
        {
                fprintf(stderr, "Failed to parse HTTP request\n");
                return -1;
        }

        snprintf(method, METHOD_SIZE, "%.*s", (int)method_len, method_ptr);
        snprintf(path, PATH_SIZE, "%.*s", (int)path_len, path_ptr);
        snprintf(body, BODY_SIZE, "%.*s", (int)(len - headers_len), buf + headers_len);

        printf("Received HTTP request: method %s, path %s, body %s\n",
               method, path, body);

        return 0;
}

/* Функция формирует HTTP-ответ и записывает его в передающий сокет @data_sock.
 * Ответ состоит из статусной строки, заголовка Content-Type с предопределенным
 * значением text/plain и тела. Используются функции send() и snprintf().
 */
static int send_http_response(int data_sock, enum http_status status, char body[])
{
        char response[1024];

        snprintf(response, sizeof(response), "HTTP/1.1 %d %s\r\n"
                                            "Content-Type: text/plain\r\n"
                                            "\r\n"
                                            "%s\r\n",
                status, status2str(status), body);
        fprintf(stderr,"Response:\n%s\n", response);
        int ret = send(data_sock, response,strlen(response), 0);
        
        if (ret < 0)
                return -1;
        return 0;
}

int main(int argc, char *argv[])
{
        int listen_sock = -1, data_sock = -1;
        struct http_string _list_head = {NULL};
        int ret;

        //Создаем слушащий сокет
        listen_sock = create_listening_socket(10000);
        if (listen_sock < 0)
                return 1;

        while (1)
        {
                int msg_len;
                char request[MESSAGE_SIZE];
                char method[METHOD_SIZE];
                char path[PATH_SIZE];
                char req_body[BODY_SIZE];

                /* Фрагмент кода принимает новое соединение и читает из него
		 * HTTP-запрос с помощью функций accept() и recv().
		 */

                data_sock = accept(listen_sock, NULL, NULL);
                if (data_sock < 0)
                {
                        fprintf(stderr, "Failed to accept data connection: %s\n",
                                strerror(errno));
                        goto on_error;
                }

                msg_len = recv(data_sock, request, sizeof(request) - 1, 0);
                if (msg_len < 0)
                {
                        fprintf(stderr, "Failed to read HTTP request: %s\n",
                                strerror(errno));
                        goto on_error;
                }
                else if (msg_len == 0)
                {
                        printf("Client disconnected\n");
                        close(data_sock);
                        break;
                }
                //Парсим реквест
                ret = parse_http_request(request, msg_len, method, path, req_body);
                if (ret < 0)
                        goto on_error;

                /* Фрагмент кода анализирует путь к ресурсу и вызывает
		 * соответствующий обработчик (handle_static() или
		 * handle_dynamic()). По результатам отправляется HTTP-ответ и
		 * закрывается передающее соединение.
                 *  
		 */
                const char static_str[] = "/static/";
                const char dyn_str[] = "/dynamic/";
                char resp_body[BODY_SIZE];
                if (!strncmp(path, static_str, strlen(static_str) - 1))
                {
                        enum http_status resp_stat = handle_static(method, path + strlen(static_str), resp_body);
                        ret = send_http_response(data_sock, resp_stat, resp_body);
                        if(ret < 0)
                                goto on_error;
                }

                data_sock = -1;
        }

        return 0;
on_error:
        if (listen_sock >= 0)
                close(listen_sock);
        if (data_sock >= 0)
                close(data_sock);
        return 1;
}
