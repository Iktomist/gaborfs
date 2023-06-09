/*
    gauge.c -- libreiserfs gauge functions
    Copyright (C) 2001, 2002 Yury Umanets <torque@ukrpost.net>, see COPYING for 
    licensing and copyright details.
*/

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <string.h>
#include <stdio.h>
#include <unistd.h>

#include <reiserfs/reiserfs.h>
#include <reiserfs/debug.h>

#define N_(String) (String)
#if ENABLE_NLS
#  include <libintl.h>
#  define _(String) dgettext (PACKAGE, String)
#else
#  define _(String) (String)
#endif

#define GAUGE_BITS_SIZE 4

static inline void default_gauge_blit(void) {
    static short bitc = 0;
    static const char bits[] = "|/-\\";

    putc(bits[bitc], stderr);
    putc('\b', stderr);
    fflush(stderr);
    bitc++;
    bitc %= GAUGE_BITS_SIZE;
}

static inline void default_gauge_header(const char *desc, 
    reiserfs_gauge_type_t type) 
{
    if (desc) {
	if (type != REISERFS_GAUGE_SILENT)
	    fprintf(stderr, "\r%s: ", desc);
	else
	    fprintf(stderr, "\r%s...", desc);
    }
}

static inline void default_gauge_footer(const char *desc, 
    reiserfs_gauge_type_t type) 
{
    if (desc)
	fputs(desc, stderr);
}

static void default_gauge_handler(const char *name, unsigned int value, 
    void *data, reiserfs_gauge_type_t type, reiserfs_gauge_state_t state)
{
    if (state == REISERFS_GAUGE_STARTED)
	default_gauge_header(name, type);
	
    switch (type) {
	case REISERFS_GAUGE_PERCENTAGE: {
	    unsigned int i;
	    char display[10] = {0};
		
	    sprintf(display, "%d%%", value);
	    fputs(display, stderr);
		
	    for (i = 0; i < strlen(display); i++)
		fputc('\b', stderr);
	    break;
	}
	case REISERFS_GAUGE_INDICATOR: {
	    default_gauge_blit();
	    break;
	}
    }

    if (state == REISERFS_GAUGE_DONE)
	default_gauge_footer(_("done\n"), type);
    
    if (state == REISERFS_GAUGE_FAILED)
	default_gauge_footer(_("failed\n"), type);
	
    fflush(stderr);
}

reiserfs_gauge_t *libreiserfs_gauge_create(reiserfs_gauge_type_t type, 
    const char *name, void *data)
{
    reiserfs_gauge_t *gauge;

    if (!(gauge = libreiserfs_calloc(sizeof(*gauge), 0)))
	return NULL;
	
    if (name) {
	int name_len = (strlen(name) < sizeof(gauge->name) ? strlen(name) : 
	    sizeof(gauge->name) - 1);
	memcpy(gauge->name, name, name_len);
    }	

    gauge->handler = default_gauge_handler;
    gauge->data = data;
    gauge->type = type;

    libreiserfs_gauge_reset(gauge);

    if (name) 
	libreiserfs_gauge_touch(gauge);
    
    return gauge;
}
	
void libreiserfs_gauge_free(reiserfs_gauge_t *gauge) {
    ASSERT(gauge != NULL, return);
    libreiserfs_free(gauge);
}

void libreiserfs_gauge_reset(reiserfs_gauge_t *gauge) {
    ASSERT(gauge != NULL, return);
	
    gauge->value = 0;
    gauge->state = REISERFS_GAUGE_STARTED;
}

void libreiserfs_gauge_set_handler(reiserfs_gauge_t *gauge, 
    reiserfs_gauge_handler_t handler) 
{
    ASSERT(gauge != NULL, return);
    gauge->handler = handler;
}

reiserfs_gauge_handler_t libreiserfs_gauge_get_handler(reiserfs_gauge_t *gauge) {
    ASSERT(gauge != NULL, return NULL);
    return gauge->handler;
}

void libreiserfs_gauge_set_type(reiserfs_gauge_t *gauge, reiserfs_gauge_type_t type) {
    ASSERT(gauge != NULL, return);
    
    if (gauge->type == type)
	return;
    
    gauge->type = type;

    if (type == REISERFS_GAUGE_INDICATOR)
	setlinebuf(stderr);
}

reiserfs_gauge_type_t libreiserfs_gauge_get_type(reiserfs_gauge_t *gauge) {
    ASSERT(gauge != NULL, return 0);
    return gauge->type;
}

void libreiserfs_gauge_set_data(reiserfs_gauge_t *gauge, void *data) {
    ASSERT(gauge != NULL, return);
    gauge->data = data;
}

void *libreiserfs_gauge_get_data(reiserfs_gauge_t *gauge) {
    ASSERT(gauge != NULL, return NULL);
    return gauge->data;
}

void libreiserfs_gauge_set_name(reiserfs_gauge_t *gauge, const char *name) {
    int name_len;

    ASSERT(gauge != NULL, return);
    ASSERT(name != NULL, return);
	
    if (!strncmp(gauge->name, name, sizeof(gauge->name)))
	return;
	
    /*name_len = (strlen(name) < sizeof(gauge->name) ? 
	strlen(name) : sizeof(gauge->name) - 1);
    
    memcpy(gauge->name, name, name_len);
    gauge->name[strlen(name)] = '\0';
*/
	strncpy(gauge->name, name, (sizeof(gauge->name)-1));
	
    gauge->state = REISERFS_GAUGE_STARTED;
    libreiserfs_gauge_touch(gauge);
}

char *libreiserfs_gauge_get_name(reiserfs_gauge_t *gauge) {
    ASSERT(gauge != NULL, return NULL);
    return gauge->name;
}

void libreiserfs_gauge_set_value(reiserfs_gauge_t *gauge, unsigned int value) {
    ASSERT(gauge != NULL, return);
	
    if (gauge->value == value)
	return;
	
    gauge->value = value;
    libreiserfs_gauge_touch(gauge);
    gauge->state = REISERFS_GAUGE_RUNNING;
}

unsigned int libreiserfs_gauge_get_value(reiserfs_gauge_t *gauge) {
    ASSERT(gauge != NULL, return 0);
    return gauge->value;
}

void libreiserfs_gauge_set_state(reiserfs_gauge_t *gauge, reiserfs_gauge_state_t state) {
    ASSERT(gauge != NULL, return);
    gauge->state = state;
}

reiserfs_gauge_state_t libreiserfs_gauge_get_state(reiserfs_gauge_t *gauge) {
    ASSERT(gauge != NULL, return 0);
    return gauge->state;
}

void libreiserfs_gauge_touch(reiserfs_gauge_t *gauge) {
    ASSERT(gauge != NULL, return);

    if (gauge->handler && gauge->state != REISERFS_GAUGE_STOPED) {
	
	gauge->handler(gauge->name, gauge->value, gauge->data, 
	    gauge->type, gauge->state);
    }
}

void libreiserfs_gauge_finish(reiserfs_gauge_t *gauge, int success) {
    ASSERT(gauge != NULL, return);

    if (gauge->state == REISERFS_GAUGE_DONE || 
	    gauge->state == REISERFS_GAUGE_FAILED)
	return;
    
    gauge->value = 100;
    gauge->state = success ? REISERFS_GAUGE_DONE : REISERFS_GAUGE_FAILED;
    libreiserfs_gauge_touch(gauge);
}

