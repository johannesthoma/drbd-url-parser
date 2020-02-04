#include <string.h>

#define container_of(ptr, type, member) \
        ((type *)( \
        (char*)(ptr) - \
        (unsigned long)(&((type *)0)->member)))

#include <linux/list.h>
#include <stdio.h>
#include <errno.h>
#include <stdbool.h>
#include <stdlib.h>
#include <ctype.h>
#include <linux/drbd_limits.h>

#define printk printf
#define kmalloc(size, unused, unused2) malloc(size)

struct net_params {
	bool use_rle;
	char *verify_alg;
	int timeout;
	int ping_timeout;
	int ping_int;
	int connect_int;
};

struct disk_params {
	int c_max_rate;
	int c_fill_target;
};

	/* for now we allow only for one volume because that is
	 * what WinDRBD boot feature needs.
	 */

struct volume {
	int volume_id;	
	int minor;
	char *disk;
	char *meta_disk;
#if 0
	wchar_t *mount_point; /* might be NULL */
#endif
};
	
struct node {
	struct list_head list;

	char *hostname;
	int node_id;
	char *address;
	struct volume volume;
};

struct drbd_params {
	struct net_params net;
	struct disk_params disk;
	struct list_head node_list;

	char *resource;
	int protocol;	/* 1=A, 2=B or 3=C */
};

/* ------------------------------------------------------------------------- */

enum token {
	TK_INVALID,
	TK_RESOURCE,
	TK_PROTOCOL,
	TK_NODE,
	TK_USE_RLE,
	TK_VERIFY_ALG,
	TK_TIMEOUT,
	TK_PING_TIMEOUT,
	TK_PING_INT,
	TK_CONNECT_INT,
	TK_C_MAX_RATE,
	TK_C_FILL_TARGET,
	TK_END,
	TK_MAX
};

static char *token_strings[TK_MAX] = {
	"",
	"resource=",
	"protocol=",
	"node",
	"use-rle=",
	"verify-alg=",
	"timeout=",
	"ping-timeout=",
	"ping-int=",
	"connect-int=",
	"c-max-rate=",
	"c-fill-target="
};

/* We use (for now) a semicolon, since the colon is also used for
 * IPv6 addresses (and for the port number).
 */

#define DRBD_CONFIG_SEPERATOR ';'

int my_atoi(const char *c)
{
	int i;

	if (c == NULL)
		return 0;

	i=0;
	while (*c >= '0' && *c <= '9') {
		i*=10;
		i+=*c-'0';
		c++;
	}
	return i;
}

/* This function intentionally does not allow for leading spaces. */

static unsigned long my_strtoul(const char *nptr, const char ** endptr, int base)
{
	unsigned long val = 0;

	while (isdigit(*nptr)) {
		val *= 10;
		val += (*nptr)-'0';
		nptr++;
	}
	if (endptr)
		*endptr = nptr;

	return val;
}

static char *my_strndup(const char *s, size_t n)
{
	char *new_string;

	new_string = kmalloc(n+1, 0, 'DRBD');
	if (new_string == NULL)
		return NULL;

	strncpy(new_string, s, n);
	new_string[n] = '\0';

	return new_string;
}



static enum token find_token(const char *s, int *index, const char **params_from, const char **params_to)
{
	enum token t;
	const char *to;

	if (*s == '\0')
		return TK_END;

	for (t=TK_INVALID+1;t<TK_END;t++) {
		size_t len = strlen(token_strings[t]);
		if (len == 0) continue;

		if (strncmp(token_strings[t], s, len) == 0) {
			if (token_strings[t][len-1] != '=') {
				*index = my_strtoul(s+len, params_from, 10);
/*				if (**params_from != '=')
					return TK_INVALID;
				(*params_from)++;
*/
			} else {
				*params_from= s+len;
			}

			to = strchr(*params_from, DRBD_CONFIG_SEPERATOR);
			if (to == NULL)
				to = strchr(*params_from, '\0');

			*params_to = to;	
			return t;
		}
	}
	return TK_INVALID;
}

void init_params(struct drbd_params *p)
{
	if (p == NULL)
		return;

	p->net.use_rle = false;
	p->net.verify_alg = NULL;
	p->net.timeout = DRBD_TIMEOUT_DEF;
	p->net.ping_timeout = DRBD_PING_TIMEO_DEF;
	p->net.ping_int = DRBD_PING_INT_DEF;
	p->net.connect_int = DRBD_CONNECT_INT_DEF;

	p->disk.c_max_rate = DRBD_C_MAX_RATE_DEF;
	p->disk.c_fill_target = DRBD_C_FILL_TARGET_DEF;

	p->resource = NULL;
	p->protocol = -1;

	INIT_LIST_HEAD(&p->node_list);
}

/* resource=<name>;protocol=<A,B or C>; ... */

#define parse_error(s) do {\
				printk("%s", s); \
				return -EINVAL; \
			} while (0);

struct node *lookup_or_create_node(struct drbd_params *p, int node_id)
{
	struct node *n;

	list_for_each_entry(struct node, n, &p->node_list, list) {
		if (n->node_id == node_id)
			return n;
	}
	n = kmalloc(sizeof(*n), 0, 'DRBD');
	if (n == NULL)
		return NULL;

	n->hostname = NULL;
	n->node_id = node_id;
	n->address = NULL;

	n->volume.volume_id = -1;
	n->volume.minor = -1;
	n->volume.disk = NULL; 
	n->volume.meta_disk = NULL;

	list_add(&n->list, &p->node_list);

	return n;
}

int parse_drbd_params_new(const char *drbd_config, struct drbd_params *params)
{
	enum token t;
	const char *params_from, *params_to, *from;
	size_t params_len;
	int index;

	init_params(params);

	if (strncmp(drbd_config, "drbd:", 5) != 0) {
		printk("Parse error: drbd URL must start with drbd:\n");
		return -1;
	}
	from = drbd_config+5;

	while (1) {
		t=find_token(from, &index, &params_from, &params_to);
		if (t == TK_INVALID)
			parse_error("Invalid token\n");

		if (t == TK_END)
			break;

		params_len = params_to-params_from;

		switch (t) {
		case TK_RESOURCE:
			if (params->resource != NULL)
				parse_error("Duplicate resource= parameter\n");

			params->resource = my_strndup(params_from, params_len);

			if (params->resource == NULL)
				parse_error("Cannot allocate memory for resource name\n");
			break;

		case TK_PROTOCOL:
		{
			char c;

			if (params->protocol != -1)
				parse_error("Duplicate protocol= parameter\n");

			c = toupper(*params_from);
			if (c < 'A' || c > 'C')
				parse_error("Protocol must be either A, B or C\n");
			params->protocol = c - 0x40;
			break;
		}
		case TK_NODE:
		{
			struct node *node;

			printk("index is %d\n", index);

			node = lookup_or_create_node(params, index);
			if (node == NULL)
				parse_error("Out of memory\n");

			switch (*params_from) {
			case '.':
				params_from++;
				t=find_token(params_from, &index, &params_from, &params_to);
printf("t is %d\n", t);
				switch (t) {
				default: break;
				}
				break;
			case '=':
				if (params_len == 0)
					parse_error("expected hostname\n");

				params_from++;
				params_len--;
				if (node->hostname != NULL)
					parse_error("Duplicate node<n>=<hostname> parameter\n");

				node->hostname = my_strndup(params_from, params_len);
				if (node->hostname == NULL)
					parse_error("Cannot allocate memory for hostname\n");
				break;
			default:
				parse_error("expected '.' or '=' after node\n");
			}


			break;
		}
		case TK_USE_RLE:
			break;

		case TK_VERIFY_ALG:
			if (params->net.verify_alg != NULL)
				parse_error("Duplicate verify-alg parameter\n");
			params->net.verify_alg = my_strndup(params_from, params_len);
			if (params->net.verify_alg == NULL)
				parse_error("Cannot allocate memory for hostname\n");
			break;

		case TK_TIMEOUT:
			params->net.timeout = my_strtoul(params_from, NULL, 10);
			break;

		case TK_PING_TIMEOUT:
			params->net.ping_timeout = my_strtoul(params_from, NULL, 10);
			break;

		case TK_PING_INT:
			params->net.ping_int = my_strtoul(params_from, NULL, 10);
			break;

		case TK_CONNECT_INT:
			params->net.connect_int = my_strtoul(params_from, NULL, 10);
			break;

		case TK_C_MAX_RATE:
			params->disk.c_max_rate = my_strtoul(params_from, NULL, 10);
			break;

		case TK_C_FILL_TARGET:
			params->disk.c_fill_target = my_strtoul(params_from, NULL, 10);
			break;

		default:
			break;
		}
		if (*params_to == '\0')
			break;

		from = params_to+1;
	}
	return 0;
}

int main(int argc, const char **argv)
{
	struct drbd_params p;
	struct node *n;

	if (argc != 2) {
		printf("Usage: %s <drbd-URL>\n", argv[0]);
		exit(1);
	}
	parse_drbd_params_new(argv[1], &p);
	
	printf("resource is %s\n", p.resource);
	printf("protocol is %d\n", p.protocol);
	list_for_each_entry(struct node, n, &p.node_list, list) {
		printf("node %d\n", n->node_id);
	}

	return 0;
}

