#include <string.h>
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
	TK_END,
	TK_MAX
};

static char *token_strings[TK_MAX] = {
	"",
	"resource=",
	"protocol=",
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

static unsigned long my_strtoul(const char *nptr, char ** endptr, int base)
{
	unsigned long val = 0;

	while (isdigit(*nptr)) {
		val *= 10;
		val += (*nptr)-'0';
		nptr++;
	}
	if (endptr)
		*endptr = (char*) nptr;

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



static enum token find_token(const char *s, const char **params_from, const char **params_to)
{
	enum token t;
	const char *to;

	if (*s == '\0')
		return TK_END;

	for (t=TK_INVALID+1;t<TK_MAX;t++) {
		size_t len = strlen(token_strings[t]);
		if (strncmp(token_strings[t], s, len) == 0) {
			*params_from= s+len;
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

	p->net.verify_alg = NULL;
	p->net.ping_timeout = DRBD_PING_TIMEO_DEF;
	p->net.ping_int = DRBD_PING_INT_DEF;

	p->resource = NULL;
	p->protocol = -1;

	INIT_LIST_HEAD(&p->node_list);
}

/* resource=<name>;protocol=<A,B or C>; ... */

#define parse_error(s) \
	printk("%s", s); \
	return -EINVAL;

int parse_drbd_params_new(const char *drbd_config, struct drbd_params *params)
{
	enum token t;
	const char *params_from, *params_to, *from;
	size_t params_len;

	init_params(params);

	if (strncmp(drbd_config, "drbd:", 5) != 0) {
		printk("Parse error: drbd URL must start with drbd:\n");
		return -1;
	}
	from = drbd_config+5;

	while (1) {
		t=find_token(from, &params_from, &params_to);
		params_len = params_to-params_from;

		switch (t) {
		case TK_RESOURCE:
			if (params->resource != NULL)
				parse_error("Duplicate resource= parameter\n");

			params->resource = my_strndup(params_from, params_len);

			if (params->resource == NULL)
				parse_error("Cannot allocate memory for resource name\n");
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

	if (argc != 2) {
		printf("Usage: %s <drbd-URL>\n", argv[0]);
		exit(1);
	}
	parse_drbd_params_new(argv[1], &p);
	
	printf("resource is %s\n", p.resource);
	return 0;
}

