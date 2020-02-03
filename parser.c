#include <string.h>
#include <linux/list.h>
#include <stdio.h>
#include <errno.h>

#define printk printf

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

/* resource=<name>;protocol=<A,B or C>; ... */

#define parse_error(s) \
	printk("%s", s); \
	return -EINVAL;

int parse_drbd_params_new(const char *drbd_config, struct drbd_params *params)
{
	enum token t;
	const char *params_from, *params_to;

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

			if (params->resource == NULL) {
				parse_error("Cannot allocate memory for resource name\n");
			break;
		}
	}
	return 0;
}

