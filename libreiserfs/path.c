/*
    path.c -- reiserfs tree path
    Copyright (C) 2001, 2002 Yury Umanets <torque@ukrpost.net>, see COPYING for 
    licensing and copyright details.
*/

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <string.h>
#include <reiserfs/reiserfs.h>
#include <reiserfs/debug.h>

/* Node functions */
reiserfs_path_node_t *reiserfs_path_node_create(reiserfs_path_node_t *parent, 
    reiserfs_block_t *node, unsigned int pos) 
{
    reiserfs_path_node_t *path_node;
	
    ASSERT(node != NULL, return NULL);
	
    if (!(path_node = libreiserfs_calloc(sizeof(*path_node), 0)))
	return NULL;
	
    path_node->pos = pos;
    path_node->node = node;
    path_node->parent = parent;
	
    return path_node;
}
	
void reiserfs_path_node_free(reiserfs_path_node_t *node) {

    ASSERT(node != NULL, return);

    reiserfs_block_free(node->node);
    libreiserfs_free(node);
}

/* Path functions */
reiserfs_path_t *reiserfs_path_create(unsigned int max_length) {
    unsigned int i;
    reiserfs_path_t *path;
	
    if (!(path = libreiserfs_calloc(sizeof(*path), 0)))
	goto error;
	
    if (!(path->nodes = libreiserfs_calloc(max_length * sizeof(void *), 0)))
	goto error_free_path;
		
    path->length = 0;
    path->max_length = max_length;

    for (i = 0; i < max_length; i++)
	path->nodes[i] = NULL;
	
    return path;
	
error_free_path:
    libreiserfs_free(path);
error:
    return NULL;
}

void reiserfs_path_clear(reiserfs_path_t *path) {

    ASSERT(path != NULL, return);
	
    while (path->length > 0)
	reiserfs_path_node_free(path->nodes[--path->length]);
}

int reiserfs_path_empty(reiserfs_path_t *path) {
    return path->length == 0;
}

void reiserfs_path_free(reiserfs_path_t *path) {
	
    ASSERT(path != NULL, return);
	
    reiserfs_path_clear(path);
	
    libreiserfs_free(path->nodes);	
    libreiserfs_free(path);
}

reiserfs_path_node_t *reiserfs_path_at(reiserfs_path_t *path, unsigned int pos) {
    ASSERT(path != NULL, return NULL);
	
    if (pos >= path->length) 
	return NULL;
	
return path->nodes[pos];
}

reiserfs_path_node_t *reiserfs_path_last(reiserfs_path_t *path) {
    ASSERT(path != NULL, return NULL);
    return reiserfs_path_at(path, path->length - 1);
}

void *reiserfs_path_last_item(reiserfs_path_t *path) {
    reiserfs_path_node_t *leaf;
	
    if (!(leaf = reiserfs_path_last(path)))
	return NULL;
	
    return get_ih_item_head(leaf->node, leaf->pos);
}

reiserfs_path_node_t *reiserfs_path_first(reiserfs_path_t *path) {
    return reiserfs_path_at(path, 0);
}

int reiserfs_path_insert(reiserfs_path_t *path, unsigned int pos, 
    reiserfs_path_node_t *node)
{
    unsigned int i;
	
    ASSERT(path != NULL, return 0);
    ASSERT(pos <= path->length, return 0);
    ASSERT(path->length < path->max_length, return 0);
	
    if (pos < path->length) {
	for (i = path->length - 1; i >= pos; i--)
	    path->nodes[i + 1] = path->nodes[i];
    }
    path->nodes[pos] = node;
    path->length++;

    return 1;
}

int reiserfs_path_remove(reiserfs_path_t *path, unsigned int pos) {
    unsigned int i;
	
    ASSERT(path != NULL, return 0);
    ASSERT(pos < path->length, return 0);

    for (i = pos + 1; i < path->length; i++)
	path->nodes[i - 1] = path->nodes[i];
	
    path->nodes[path->length--] = NULL;

    return 1;
}
	
int reiserfs_path_inc(reiserfs_path_t *path, reiserfs_path_node_t *node) {
    return reiserfs_path_insert(path, path->length, node);
}

int reiserfs_path_dec(reiserfs_path_t *path) {
    return reiserfs_path_remove(path, path->length - 1);
}

reiserfs_path_node_t *reiserfs_path_pop(reiserfs_path_t *path) {
    reiserfs_path_node_t *node;
	
    node = path->nodes[path->length - 1];
    path->nodes[path->length - 1] = NULL;
    return node;
}

