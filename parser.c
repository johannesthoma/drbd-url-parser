#include <string.h>

static struct drbd_params {
	char *resource;
	int num_nodes;
	int minor;
	int volume;
	wchar_t *mount_point; /* might be NULL */
	char *peer;
	int peer_node_id;
	int protocol;	/* 1=A, 2=B or 3=C */
	char *my_address;
	char *peer_address;
} boot_devices[1] = {
	{
		.resource = "new-windows",
		.num_nodes = 2,
		.minor = 1,
		.volume = 1,
		.mount_point = NULL, 
		.peer = "johannes-VirtualBox",
		.peer_node_id = 1,
		.protocol = 3,
		.my_address = "0.0.0.0:7691",
		.peer_address = "192.168.56.102:7691"
	}

enum tokens {
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

enum token find_token(const char *s, const char **params_from, const char **params_to)
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
	printf("%s", s); \
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

	while (..) {
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

