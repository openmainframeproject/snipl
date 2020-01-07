/*
 * sncapdsm.c : API for snCAP XML Document Structure Model
 *
 * Copyright IBM Corp. 2012, 2013
 *
 * Published under the terms and conditions of the CPL (common public license)
 *
 * PLEASE NOTE:
 *   config is provided under the terms of the enclosed common public license
 *   ("agreement). Any use, reproduction or distribution of the program
 *   constitues recipient's acceptance of this agreement.
 */
#include "sncapdsm.h"

/*
 *	function: dsm_list_add
 *
 *	purpose: add a dsm_node to the head of the dsm node list.
 */
void dsm_list_add(struct dsm_list *list, struct dsm_node *item)
{
	if (list->root == NULL)
		list->root = item;

	item->prev = list->tail;
	item->next = NULL;
	if (list->tail != NULL)
		list->tail->next = item;
	list->tail = item;
	list->n_items += 1;
}

/*
 *	function: dsm_list_find
 *
 *	purpose: find a node in the XML node list by the XML tag name.
 */
struct dsm_node *dsm_list_find(const struct dsm_list *list, const char *name)
{
	struct dsm_node *c = NULL;

	for (c = list->root; c != NULL; c = c->next)
		if (strcasecmp(c->name, name) == 0)
			return c;
	return NULL;
}

/*
 *	fuction: dsm_node_init
 *
 *	purpose: initialize a Document Structure Model tree node.
 */
int dsm_node_init(struct dsm_node *n, const char *name,	const char *value)
{
	n->next = NULL;
	n->prev = NULL;
	n->children.n_items = 0;
	n->children.root = NULL;
	n->children.tail = NULL;

	n->name = NULL;
	if (name != NULL) {
		n->name = strdup(name);
		if (!n->name)
			return DSM_MEMORY_ERROR;
	}
	n->value = NULL;
	if (value != NULL) {
		n->value = strdup(value);
		if (!n->value)
			return DSM_MEMORY_ERROR;
	}
	return DSM_OK;
}

/*
 *	function: return_root
 *
 *	purpose: a helper function returning the root node address to caller
 *		in the first item of the resulting pointer array.
 */
static int return_root(struct dsm_node *root,
			int *n_nodes,
			struct dsm_node ***nodes)
{
	*n_nodes = 1;
	*nodes = calloc(*n_nodes, sizeof(struct dsm_node *));
	if (!*nodes)
		return DSM_MEMORY_ERROR;
	(*nodes)[0] = root;
	return DSM_OK;
}
/*
 *      function: dsm_get_all
 *
 *      purpose: Create array of pointers to the all the nodes described by
 *              the path parameter.
 */
int dsm_get_all(struct dsm_node *root,
		const char *path,
		int *n_nodes,
		struct dsm_node ***nodes)
{
	char *subnode_name = NULL;
	char *temp_path = NULL;
	const struct dsm_node *curr_node = NULL;
	struct dsm_node *c = NULL;
	int ret = DSM_OK;
	int i = 0;
	char *child_name = NULL;

	/*
	 * If an empty path has been specified, return pointer to root node in
	 * an array of 1 item.
	 */
	curr_node = root;
	if (strlen(path) == 0) {
		ret = return_root(root, n_nodes, nodes);
		goto clean_up;
	}
	temp_path = strdup(path);
	if (!temp_path) {
		ret = DSM_MEMORY_ERROR;
		goto clean_up;
	}
	/*
	 * Get the name of the target tag.
	 */
	child_name = strrchr(temp_path, '/');
	/*
	 * If only the root tag name has been specified assume that we should
	 * return only the pointer to the root node.
	 */
	if (!child_name) {
		ret = return_root(root, n_nodes, nodes);
		goto clean_up;
	}
	*child_name++ = '\0';
	/*
	 * Find the deepest parent node.
	 */
	subnode_name = strtok(temp_path, "/");
	if (strcasecmp(curr_node->name, root->name) != 0)
		return DSM_INVALID_PATH;
	subnode_name = strtok(NULL, "/");
	while (subnode_name) {
		curr_node = dsm_list_find(&curr_node->children,
				subnode_name);
		if (curr_node == NULL) {
			ret = DSM_INVALID_PATH;
			goto clean_up;
		}
		subnode_name = strtok(NULL, "/");
	}
	/*
	 * Calculate the number of direct children with the given name.
	 */
	c = curr_node->children.root;
	*n_nodes = 0;
	for (c = curr_node->children.root; c != NULL; c = c->next)
		if (strcasecmp(child_name, c->name) == 0)
				(*n_nodes)++;

	*nodes = calloc(*n_nodes, sizeof(struct dsm_node *));
	if (!*nodes) {
		ret = DSM_MEMORY_ERROR;
		goto clean_up;
	}
	/*
	 * Fill the array with the pointers to situable nodes.
	 */
	for (c = curr_node->children.root; c != NULL; c = c->next)
		if (strcasecmp(child_name, c->name) == 0)
			(*nodes)[i++] = c;
clean_up:
	free(temp_path);
	return ret;
}

/*
 *      function: dsm_get_value_string
 *
 *      purpose: retrieve the text value for the DSM Tree node specified by
 *               path and node name.
 */
int dsm_get_value_string(const struct dsm_node *root,
			const char *path,
			char **value)
{
	const struct dsm_node *curr_node = NULL;
	const char *subnode_name = NULL;
	char *temp_path = NULL;
	int ret = DSM_OK;

	curr_node = root;
	temp_path = strdup(path);
	if (!temp_path)
		return DSM_MEMORY_ERROR;

	subnode_name = strtok(temp_path, "/");
	subnode_name = strtok(NULL, "/");
	while (subnode_name) {
		curr_node = dsm_list_find(&curr_node->children,	subnode_name);
		if (curr_node == NULL) {
			ret = DSM_INVALID_PATH;
			goto cleanup;
		}
		subnode_name = strtok(NULL, "/");
	}

	if (!curr_node->value) {
		ret = DSM_NO_VALUE;
		goto cleanup;
	}
	*value = strdup(curr_node->value);
	if (!*value)
		ret = DSM_MEMORY_ERROR;
cleanup:
	free(temp_path);

	return ret;
}

/*
 *      function: dsm_get_value_int
 *
 *      purpose: retrieve the int value for the DSM Tree node specified by
 *               path and node path.
 */
int dsm_get_value_int(const struct dsm_node *root,
		const char *path,
		int *value)
{
	char *valbuf = NULL;
	int ret = DSM_OK;

	ret = dsm_get_value_string(root, path, &valbuf);
	if (ret != DSM_OK)
		return ret;

	ret = sscanf(valbuf, "%d", value);
	if (ret != 1) {
		ret = DSM_INVALID_VALUE;
		goto cleanup;
	}
	ret = DSM_OK;
cleanup:
	free(valbuf);
	return ret;
}

/*
 *	function: is_end_tag
 *
 *	purpose: check if a tag is a closing tag
 */
static int is_end_tag(const char *tag)
{
	return tag[0] == '/';
}

/*
 * States of the finit state automaton with the stack option
 * used for pseudo-XML parsing.
 */
enum dsm_xml_state {
	XML_START = 1,
	XML_TAG_START,
	XML_TAG_BODY,
	XML_TAG_END,
	XML_VALUE,
	XML_ERROR
	};

/*
 * Internal buffer structure used by the pseudo-XML parser.
 */
struct dsm_parser_buffer {
	int size;
	int curr_pos;
	char *data;
};

/*
 *	function: dsm_parser_buffer_alloc
 *
 *	purpose: allocate an internal parser buffer
 */
int dsm_parser_buffer_alloc(struct dsm_parser_buffer *b, const int max_size)
{
	b->size = max_size + 1;
	b->curr_pos = 0;
	b->data = calloc(b->size, sizeof(*b->data));

	if (b->data == NULL)
		return DSM_MEMORY_ERROR;
	return DSM_OK;
}

/*
 *	function: dsm_parser_buffer_reset
 *
 *	purpose: reset (set empty) an internal parser buffer.
 */
static void dsm_parser_buffer_reset(struct dsm_parser_buffer *b)
{
	b->curr_pos = 0;
	if (b->data != NULL)
		memset(b->data, '\0', b->size);
}

/*
 * The "active" XML node stack used by the pseudo-XML parser.
 */
struct dsm_node_stack {
	struct dsm_node *data[DSM_STACK_DEPTH];
	int head;
};

/*
 * Document Strucutre Model XML parser state structure
 */
struct dsm_state {
	struct dsm_node **root;
	struct dsm_node_stack stack;
};

/*
 *	function: dsm_init
 *
 *	purpose: initialize DSM XML parser state structure.
 */
static void dsm_init(struct dsm_state *d, struct dsm_node **root)
{
	d->root = root;
	d->stack.head = -1;
}

/*
 *	function: dsm_open_node
 *
 *	purpose: process current opening node.
 */
static int dsm_open_node(struct dsm_state *d, const char *node_name)
{
	struct dsm_node *c = NULL;

	c = calloc(1, sizeof(*c));
	dsm_node_init(c, node_name, NULL);
	if (d->stack.head < 0) {
		d->stack.head = 0;
		d->stack.data[d->stack.head] = c;
	} else {
		dsm_list_add(&d->stack.data[d->stack.head]->children, c);
		if (d->stack.head < DSM_STACK_DEPTH) {
			d->stack.head += 1;
			d->stack.data[d->stack.head] = c;
		}
	}
	return DSM_OK;
}

/*
 *	function: dsm_close_node
 *
 *	purpose: process closing XML tag
 */
static int dsm_close_node(struct dsm_state *d, const char *node_name)
{
	struct dsm_node *c = NULL;

	if (d->stack.head < 0)
		return DSM_SYNTAX_ERROR;

	c = d->stack.data[d->stack.head];
	if (strcmp(c->name, &node_name[1]) != 0)
		return DSM_SYNTAX_ERROR;

	if ((d->stack.head - 1) < 0)
		*d->root = d->stack.data[d->stack.head--];
	d->stack.head -= 1;

	return DSM_OK;
}

/*
 *	function: dsm_create_value
 *
 *	purpose: create value for the stack head.
 */
static int dsm_create_value(struct dsm_state *d, const char *value)
{
	int len = 0;
	char *value_buffer;

	if (d->stack.head < 0)
		return DSM_SYNTAX_ERROR;

	if (d->stack.data[d->stack.head]->value) {
		len = strlen(d->stack.data[d->stack.head]->value);
		len += strlen(value) + 1;
		value_buffer = calloc(len, sizeof(*value));
		if (!value_buffer)
			return DSM_MEMORY_ERROR;
		strcpy(value_buffer, d->stack.data[d->stack.head]->value);
		strcat(value_buffer, value);
		free(d->stack.data[d->stack.head]->value);
	} else {
		value_buffer = strdup(value);
		if (!value_buffer)
			return DSM_MEMORY_ERROR;
	}

	d->stack.data[d->stack.head]->value = value_buffer;

	return DSM_OK;
}

/*
 * DSM XML parser internam data
 */
struct dsm_parser {
	enum dsm_xml_state state;
	struct dsm_parser_buffer buffer;
	struct dsm_state dom;
};

/*
 *	function: dsm_parser_init
 *
 *	purpose: initialize an XML parser
 */
static void dsm_parser_init(struct dsm_parser *p,
			const int input_len,
			struct dsm_node **root)
{
	p->state = XML_START;

	dsm_parser_buffer_alloc(&p->buffer, input_len);
	dsm_init(&p->dom, root);
}

/*
 *	function: dsm_parser_buffer
 *
 *	purpose: put a character into an XML parser internal buffer.
 */
static int dsm_parser_buffer_put(struct dsm_parser_buffer *b, const char input)
{
	if (b->curr_pos + 1 < b->size - 1)
		b->data[b->curr_pos++] = input;
	else
		return DSM_BUFFER_OVERFLOW;
	return DSM_OK;
}

/*
 *	function: handle_tag_start
 *
 *	purpose: process the tag start state of the automaton.
 */
static int handle_tag_start(struct dsm_parser *p, const char input)
{
	int ret = DSM_OK;

	switch (p->state) {
	case XML_START:
		dsm_parser_buffer_reset(&p->buffer);
		p->state = XML_TAG_START;
		break;
	case XML_TAG_END:
		ret = dsm_create_value(&p->dom, p->buffer.data);
		if (ret != DSM_OK)
			break;
		dsm_parser_buffer_reset(&p->buffer);
		p->state = XML_TAG_START;
		break;
	case XML_VALUE:
		ret = dsm_create_value(&p->dom, p->buffer.data);
		if (ret != DSM_OK)
			break;
		dsm_parser_buffer_reset(&p->buffer);
		p->state = XML_TAG_START;
		break;
	default:
		p->state = XML_ERROR;
	}
	return ret;
}

/*
 *	fucntion: handle_tag_end
 *
 *	purpose: process tag end state of the automaton.
 */
static int handle_tag_end(struct dsm_parser *p,	const char input)
{
	int ret = DSM_OK;

	switch (p->state) {
	case XML_TAG_BODY:
		if (is_end_tag(p->buffer.data))
			ret = dsm_close_node(&p->dom, p->buffer.data);
		else
			ret = dsm_open_node(&p->dom, p->buffer.data);
		if (ret != DSM_OK)
			break;
		dsm_parser_buffer_reset(&p->buffer);
		p->state = XML_TAG_END;
		break;
	default:
		p->state = XML_ERROR;
	}
	return ret;
}

/*
 *	function: handle_slash
 *
 *	purpose: process the slash charachter that has a special meaning
 *		 in the XML.
 */
static int handle_slash(struct dsm_parser *p, const char input)
{
	int ret = DSM_OK;

	switch (p->state) {
	case XML_TAG_START:
		ret = dsm_parser_buffer_put(&p->buffer, input);
		p->state = XML_TAG_BODY;
		break;
	case XML_TAG_BODY:
		p->state = XML_ERROR;
		break;
	default:
		ret = dsm_parser_buffer_put(&p->buffer, input);
	}
	return ret;
}

/*
 *	function: handle letter
 *
 *	purpose: process a regual character
 */
static int handle_letter(struct dsm_parser *p, const char input)
{
	int ret = DSM_OK;

	if (isspace(input)) {
		switch (p->state) {
		case XML_TAG_START:
			p->state = XML_ERROR;
			break;
		case XML_TAG_END:
			break;
		default:
			ret = dsm_parser_buffer_put(&p->buffer, input);
			break;
		}
	} else {
		switch (p->state) {
		case XML_START:
			p->state = XML_ERROR;
			break;
		case XML_TAG_START:
			ret = dsm_parser_buffer_put(&p->buffer, input);
			p->state = XML_TAG_BODY;
			break;
		case XML_TAG_BODY:
			ret = dsm_parser_buffer_put(&p->buffer, input);
			break;
		case XML_TAG_END:
			ret = dsm_parser_buffer_put(&p->buffer, input);
			p->state = XML_VALUE;
			break;
		case XML_VALUE:
			ret = dsm_parser_buffer_put(&p->buffer, input);
			break;
		default:
			break;
		}
	}
	return ret;
}

/*
 *	function: dsm_parse_xml
 *
 *	purpose: parse an input XML string and create DSM Tree from the
 *		 node root.
 */
int dsm_parse_xml(const char *input, struct dsm_node **root)
{
	int i = 0;
	int input_len = 0;
	int ret = DSM_OK;

	struct dsm_parser parser;

	dsm_parser_init(&parser, strlen(input), root);

	input_len = strlen(input);
	for (i = 0; i < input_len; i++) {
		switch (input[i]) {
		case '<':
			ret = handle_tag_start(&parser, input[i]);
			break;

		case '>':
			ret = handle_tag_end(&parser, input[i]);
			break;

		case '/':
			ret = handle_slash(&parser, input[i]);
			break;

		default:
			ret = handle_letter(&parser, input[i]);
		}
		if (parser.state == XML_ERROR) {
			ret = DSM_SYNTAX_ERROR;
			break;
		}

	}
	free(parser.buffer.data);
	return ret;
}

/*
 *	function: dsm_tree_release
 *
 *	purpose: release memory used by the DSM tree node items.
 */
void dsm_tree_release(struct dsm_node *root)
{
	struct dsm_node *c = NULL;

	if (!root)
		return;

	c = root->children.root;
	while (c != NULL) {
		dsm_tree_release(c);
		root->children.root = c->next;
		free(c);
		c = root->children.root;
	}
	free(root->name);
	free(root->value);
}
