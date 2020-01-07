/*
 * sncapdsm.h : header file for SNCAP Document Structure Model
 *
 * Copyright IBM Corp. 2012, 2013
 *
 * Published under the terms and conditions of the CPL (common public license)
 *
 * PLEASE NOTE:
 *   config is provided under the terms of the enclosed common public license
 *   ("agreement"). Any use, reproduction or distribution of the program
 *   constitutes recipient's acceptance of this agreement.
 */
#ifndef SNCAPDSMH
#define SNCAPDSMH
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define DSM_STACK_DEPTH 20

struct dsm_node;

enum dsm_parser_state {
	DSM_OK,
	DSM_VALUE_FOUND,
	DSM_NO_VALUE,
	DSM_SYNTAX_ERROR,
	DSM_INVALID_PATH,
	DSM_INVALID_NODE_NAME,
	DSM_MEMORY_ERROR,
	DSM_BUFFER_OVERFLOW,
	DSM_INVALID_VALUE
};
/*
 * Double-linked list structure for an XML node children.
 */
struct dsm_list {
	int n_items;
	struct dsm_node *root;
	struct dsm_node *tail;
};

/*
 *	function: dsm_list_find
 *
 *	purpose: search the list for a node with given node name name.
 */
struct dsm_node *dsm_list_find(const struct dsm_list *list,
		 const char *name);

/*
 *	function: dsm_list_add
 *
 *	purpose: add a dsm_node item to the head of the dsm_list list.
 */
void dsm_list_add(struct dsm_list *list, struct dsm_node *item);

/*
 * Parsed XML node structure.
 */
struct dsm_node {
	struct dsm_node *next;
	struct dsm_node *prev;
	char *name;
	char *value;
	struct dsm_list children;
};

/*
 *	function: dsm_node_init
 *
 *	purpose: initialize the dsm_node n, with the node name and node value.
 */
int dsm_node_init(struct dsm_node *n, const char *name, const char *value);

/*
 *	function: dsm_get_all
 *
 *	purpose: Create array of pointers to the all the nodes described by
 *		the path parameter.
 */
int dsm_get_all(struct dsm_node *root,
		const char *path,
		int *n_nodes,
		struct dsm_node ***nodes);

/*
 *	function: dsm_node_get_value_string
 *
 *	purpose: retrieve the text value for the DSM Tree node specified by
 *		 path and node path.
 */
int dsm_get_value_string(const struct dsm_node *root,
		const char *path,
		char **value);

/*
 *	function: dsm_node_get_value_int
 *
 *	purpose: retrieve the int value for the DSM Tree node specified by
 *		 path and node path.
 */
int dsm_get_value_int(const struct dsm_node *root,
		const char *path,
		int *value);

/*
 *	function: dsm_get_value
 *
 *	purpose: return the text value for a child of the DSM Tree node
 *		 specified by path using index.
 */
char *dsm_get_value(const struct dsm_node *root,
		const char *path,
		const int index);

/*
 *	function: dsm_parse_xml
 *
 *	purpose: parse the input pseudo-XML string and creates the DSM tree.
 *
 *	Note: the parser was implemented using the "Finite State Atomaton with
 *	      Stack" model.
 */
int dsm_parse_xml(const char *input, struct dsm_node **root);

/*
 *	function: dsm_tree_release
 *
 *	purpose: release memory used by the DSM tree nodes.
 */
void dsm_tree_release(struct dsm_node *root);
#endif
