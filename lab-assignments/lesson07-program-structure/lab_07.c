
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>
#include <glob.h>
#include <errno.h>
#include <sys/signal.h>

#include <libevdev-1.0/libevdev/libevdev.h>
#include <libubox/list.h>
#include <libubox/avl.h>
#include <libubox/avl-cmp.h>

#define BUF_SIZE 128

typedef int (*read_event_fn_t)(char value[], void *dev);
typedef int (*handle_event_fn_t)(void *dev);
struct device
{
	struct avl_node anode;
	char *name;
	struct libevdev *evdev;
	handle_event_fn_t handle_event_callback;
	read_event_fn_t read_event_callback;
	void *callback_data;
};

struct power_button
{
	struct libevdev *evdev;
};

struct mouse
{
	struct libevdev *evdev;
};

struct key
{
	struct list_head list_entry;
	char key_char;
	struct timeval tv;
};

struct keyboard
{
	struct libevdev *evdev;
	struct list_head key_list;
};

static bool terminate;

static int read_power_button_event(char value[], void *dev)
{
	struct power_button *button = dev;
	struct input_event ev;
	int ret;

	while (1)
	{
		ret = libevdev_next_event(button->evdev, LIBEVDEV_READ_FLAG_NORMAL, &ev);
		if (ret == -EAGAIN)
		{
			fprintf(stderr, "Power button event not on list\n");
			return 0;
		}
		else if (ret < 0)
		{
			fprintf(stderr, "Failed to read event from power button\n");
			return -1;
		}
		if (ev.type == EV_KEY && ev.code == KEY_POWER)
		{
			snprintf(value, BUF_SIZE, "Power button pressed");
			break;
		}
	}

	return 0;
}

/* Функция последовательно просматривает события от клавиатуры с помощью
 * функции libevdev_next_event(). Для первого события, соответствующего нажатию
 * левой или правой кнопки мыши, программа выводит сообщение в строку @value и
 * завершается.
 */
static int read_mouse_event(char value[], void *dev)
{

	struct mouse *mouse = dev;
	struct input_event ev;
	int ret;
	while (1)
	{
		ret = libevdev_next_event(mouse->evdev, LIBEVDEV_READ_FLAG_NORMAL, &ev);
		if (ret == -EAGAIN)
		{
			fprintf(stderr, "Mouse event not on list\n");
			return 0;
		}
		else if (ret < 0)
		{
			fprintf(stderr, "Failed to read event from mouse\n");
			return -1;
		}
		if (ev.type == EV_KEY && ev.code == BTN_LEFT && ev.value == 1)
		{
			snprintf(value, BUF_SIZE, "Left key was pressed!");
			break;
		}
		else if (ev.type == EV_KEY && ev.code == BTN_RIGHT && ev.value == 1)
		{
			snprintf(value, BUF_SIZE, "Right key was pressed!");
			break;
		}
	}

	return 0;
}

/* Если очередь событий key_list не пуста, функция извлекает первый элемент
 * очереди, выводит описание события в строку @value, после чего удаляет
 * извлеченный элемент. Используются макросы list_empty(), list_first_entry(),
 * list_del().
 */
static int read_keyboard_event(char value[], void *dev)
{
	struct keyboard *kboard = dev;
	struct key *key;
	

	if (!list_empty(&kboard->key_list))
	{
		key = list_first_entry(&kboard->key_list, struct key, list_entry);
		snprintf(value, BUF_SIZE, "Entered key '%c' at %ld seconds since Epoch", key->key_char, key->tv.tv_sec);
		list_del(&key->list_entry);
		free(key);
	}
	else
	{
		snprintf(value, BUF_SIZE, "Keyboard list empty");
	}
	return 0;
}

/* Функция последовательно просматривает события от клавиатуры с помощью
 * функции libevdev_next_event(). Если событие является нажатием клавиши с
 * цифрой, программа создает новый объект struct key, записывает в него код
 * события и текущее время, после чего добавляет объект в конец очереди с
 * помощью макроса list_add_tail().
 */
static int handle_keyboard_event(void *dev)
{
	struct keyboard *kboard = dev;
	struct input_event ev;
	int ret;
	while (1)
	{
		ret = libevdev_next_event(kboard->evdev, LIBEVDEV_READ_FLAG_NORMAL, &ev);
		if (ret == -EAGAIN)
		{
			return 0;
		}
		if (ret < 0)
		{
			fprintf(stderr, "Failed to read event from keyboard\n");
			return -1;
		}
		/* Find a pressing of digit. */
		if (ev.type == EV_KEY &&
		    ev.code >= KEY_1 && ev.code <= KEY_0 &&
		    ev.value == 1)
		{
			
			struct key *key;
			key = calloc(1,sizeof(*key));
			key->key_char = (ev.code == 11) ? '0' : ev.code - 1 + '0';
			gettimeofday(&key->tv, NULL);
			list_add_tail(&key->list_entry, &kboard->key_list);
		}
	}
	//printf("Entered key '%c' at %ld seconds since Epoch\n", key.key_char, key.tv.tv_sec);

	return 0;
}

/* Функция создает и инициализирует новый объект struct device, после чего
 * добавляет его в словарь устройств с помощью функции avl_insert(). Ключем
 * является имя устройства.
 */
static int register_device(struct avl_tree *devices,
			   const char *name,
			   struct libevdev *evdev,
			   handle_event_fn_t handle_event,
			   read_event_fn_t read_event,
			   void *data)
{
	struct device *dev = NULL;
	int ret;

	dev = calloc(1, sizeof(*dev));
	if (!dev)
		goto on_error;

	dev->name = strdup(name);
	if (!dev->name)
		goto on_error;
	dev->anode.key = dev->name;

	dev->evdev = evdev;
	dev->read_event_callback = read_event;
	dev->handle_event_callback = handle_event;
	dev->callback_data = data;
	ret = avl_insert(devices, &dev->anode);
	if (ret < 0)
	{
		fprintf(stderr, "Found duplicate of device '%s'\n", name);
		goto on_error;
	}
	return 0;
on_error:
	if (dev)
		free(dev->name);
	free(dev);
	return -1;
}

static void cleanup_devices(struct avl_tree *devices)
{
	struct device *dev, *next_dev;

	avl_for_each_element_safe(devices, dev, anode, next_dev)
	{
		int fd = libevdev_get_fd(dev->evdev);

		if (!strncmp(dev->name, "keyboard", strlen("keyboard")))
		{
			struct keyboard *kboard = dev->callback_data;
			struct key *key, *next_key;

			list_for_each_entry_safe(key, next_key,
						 &kboard->key_list, list_entry)
			{
				list_del(&key->list_entry);
				free(key);
			}
		}
		avl_delete(devices, &dev->anode);
		libevdev_free(dev->evdev);
		close(fd);
		free(dev->callback_data);
		free(dev->name);
		free(dev);
	}
}

static int scan_devices(struct avl_tree *devices)
{
	glob_t gl = {0};
	int ret;
	int pathi;

	ret = glob("/dev/input/event*", 0, NULL, &gl);
	if (ret != 0)
	{
		fprintf(stderr, "Didn't find any event device\n");
		goto on_error;
	}

	for (pathi = 0; pathi < gl.gl_pathc; pathi++)
	{
		int fd;
		struct libevdev *evdev;

		fd = open(gl.gl_pathv[pathi], O_RDONLY | O_NONBLOCK);
		if (fd < 0)
		{
			fprintf(stderr, "Failed to open '%s': %s\n",
				gl.gl_pathv[pathi], strerror(errno));
			goto on_error;
		}

		ret = libevdev_new_from_fd(fd, &evdev);
		if (ret < 0)
		{
			fprintf(stderr, "Failed to init evdev: %s\n", strerror(errno));
			goto on_error;
		}
		if (libevdev_has_event_type(evdev, EV_KEY) &&
		    libevdev_has_event_code(evdev, EV_KEY, KEY_POWER) &&
		    !libevdev_has_event_code(evdev, EV_KEY, KEY_0))
		{
			struct power_button *button;

			button = calloc(1, sizeof(*button));
			if (!button)
				return -1;
			button->evdev = evdev;
			ret = register_device(devices, "power-button", evdev,
					      NULL, read_power_button_event, button);
			if (ret < 0)
				free(button);
		}
		else if (libevdev_has_event_type(evdev, EV_REL) &&
			 libevdev_has_event_type(evdev, EV_ABS))
		{
			struct mouse *mouse;

			mouse = calloc(1, sizeof(*mouse));
			if (!mouse)
				return -1;
			mouse->evdev = evdev;
			ret = register_device(devices, "mouse", evdev,
					      NULL, read_mouse_event, mouse);
			if (ret < 0)
				free(mouse);
		}
		else if (libevdev_has_event_type(evdev, EV_KEY) &&
			 libevdev_has_event_code(evdev, EV_KEY, KEY_0))
		{
			struct keyboard *kboard;

			kboard = calloc(1, sizeof(*kboard));
			if (!kboard)
				return -1;
			kboard->evdev = evdev;
			INIT_LIST_HEAD(&kboard->key_list);
			ret = register_device(devices, "keyboard", evdev,
					      handle_keyboard_event,
					      read_keyboard_event, kboard);
			if (ret < 0)
				free(kboard);
		}
		else
		{
			libevdev_free(evdev);
			close(fd);
		}
		if (ret < 0)
		{
			libevdev_free(evdev);
			close(fd);
		}
	}
	globfree(&gl);
	return 0;
on_error:
	globfree(&gl);
	return -1;
}

/* Функция разбивает строку @command на имя команды и аргумент. Программа
 * поддерживает единственную команду read. Если аргументом является слово all,
 * программа выводит первое событие из очереди событий каждого устройства (или
 * сообщает, что событий нет), в противном случае программа считает аргумент
 * именем устройства и выводит событие только для него.
 */
static int execute_command(struct avl_tree *devices, const char *command)
{
	char cmd[BUF_SIZE], arg[BUF_SIZE], response[BUF_SIZE];
	int ret;
	struct device *dev;

	memset(cmd, '\0', BUF_SIZE);
	memset(arg, '\0', BUF_SIZE);
	memset(response, '\0', BUF_SIZE);
	ret = sscanf(command, "%s %s", cmd, arg);
	if (ret < 2)
	{
		fprintf(stderr, "Invalid input!\n");
		return -1;
	}

	if (!strcmp(cmd, "read") && !strcmp(arg, "all"))
	{
		avl_for_each_element(devices, dev, anode)
		{
			ret = dev->read_event_callback(response, dev->callback_data);
			printf("%s\n", response);
			memset(response, '\0', BUF_SIZE);
		}
	}
	else if (!strcmp(cmd, "read") && !strcmp(arg, "mouse"))
	{
		dev = avl_find_element(devices, "mouse", dev, anode);
		ret = dev->read_event_callback(response, dev->callback_data);
		printf("%s\n", response);
		memset(response, '\0', BUF_SIZE);
	}
	else if (!strcmp(cmd, "read") && !strcmp(arg, "power-button"))
	{

		dev = avl_find_element(devices, "power-button", dev, anode);
		ret = dev->read_event_callback(response, dev->callback_data);
		printf("%s\n", response);
		memset(response, '\0', BUF_SIZE);
	}
	else if (!strcmp(cmd, "read") && !strcmp(arg, "keyboard"))
	{
		fprintf(stderr, "Reading keyboard\n");
		dev = avl_find_element(devices, "keyboard", dev, anode);
		ret = dev->read_event_callback(response, dev->callback_data);
		printf("%s\n", response);
		memset(response, '\0', BUF_SIZE);
	}
	else
	{
		fprintf(stderr, "Invalid input!\n");
	}

	return 0;
}

static void sigterm_handler(int sig)
{
	terminate = true;
}

int main(int argc, char *argv[])
{
	struct avl_tree _devices, *devices = &_devices;
	int ret;

	avl_init(devices, avl_strcmp, false, NULL);

	ret = scan_devices(devices);
	if (ret < 0)
		return 1;

	signal(SIGTERM, sigterm_handler);
	signal(SIGINT, sigterm_handler);

	while (!terminate)
	{
		fd_set fds;
		int max_fd = 0;
		struct device *dev;
		int fd;

		FD_ZERO(&fds);
		FD_SET(STDIN_FILENO, &fds);
		max_fd = STDIN_FILENO;

		/* Фрагмент кода добавляет в множество fds дескрипторы тех
		 * устройств, которые зарегистрировали обратный вызов
		 * handle_event_callback.
		 */

		avl_for_each_element(devices, dev, anode)
		{
			if (dev->handle_event_callback != NULL)
			{
				fd = libevdev_get_fd(dev->evdev);
				FD_SET(fd, &fds);
				max_fd = (fd > max_fd) ? fd + 1 : max_fd;
			}
		}

		ret = select(max_fd + 1, &fds, NULL, NULL, NULL);
		if (ret < 0 && errno == EINTR)
		{
			continue;
		}
		else if (ret < 0)
		{
			fprintf(stderr, "select() failed: %s\n", strerror(errno));
			goto on_error;
		}

		if (FD_ISSET(STDIN_FILENO, &fds))
		{
			char command[BUF_SIZE];

			if (fgets(command, sizeof(command), stdin) == NULL)
				goto on_error;
			ret = execute_command(devices, command);
		}

		/* Фрагмент кода вызывает обратный вызов handle_event_callback
		 * для устройств с готовым файловым дескриптором.
		 */

		avl_for_each_element(devices, dev, anode)
		{
			if (FD_ISSET(libevdev_get_fd(dev->evdev), &fds))
			{
				ret = dev->handle_event_callback(dev->callback_data);
			}
		}
	}

	cleanup_devices(devices);
	return 0;
on_error:
	cleanup_devices(devices);
	return 1;
}
