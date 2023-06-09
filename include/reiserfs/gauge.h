/*
    gauge.h -- libreiserfs gauge functions
    Copyright (C) 2001, 2002 Yury Umanets <torque@ukrpost.net>, see COPYING for 
    licensing and copyright details.
*/

#ifndef GAUGE_H
#define GAUGE_H

enum reiserfs_gauge_type {
    REISERFS_GAUGE_PERCENTAGE,
    REISERFS_GAUGE_INDICATOR,
    REISERFS_GAUGE_SILENT
};

typedef enum reiserfs_gauge_type reiserfs_gauge_type_t;

enum reiserfs_gauge_state {
    REISERFS_GAUGE_STARTED,
    REISERFS_GAUGE_RUNNING,
    REISERFS_GAUGE_STOPED,
    REISERFS_GAUGE_FAILED,
    REISERFS_GAUGE_DONE
};

typedef enum reiserfs_gauge_state reiserfs_gauge_state_t;

typedef struct reiserfs_gauge reiserfs_gauge_t;

typedef void (*reiserfs_gauge_handler_t)(const char *, 
    unsigned int, void *, reiserfs_gauge_type_t, reiserfs_gauge_state_t);

typedef enum reiserfs_gauge_kind reiserfs_gauge_kind_t;

struct reiserfs_gauge {
    int state;
    void *data;
    char name[256];
    unsigned int value;
    
    reiserfs_gauge_type_t type;
    reiserfs_gauge_handler_t handler;
};

extern reiserfs_gauge_t *libreiserfs_gauge_create(reiserfs_gauge_type_t type, 
    const char *name, void *data);

extern void libreiserfs_gauge_free(reiserfs_gauge_t *gauge);

extern void libreiserfs_gauge_set_handler(reiserfs_gauge_t *gauge, 
    reiserfs_gauge_handler_t handler);

extern reiserfs_gauge_handler_t libreiserfs_gauge_get_handler(reiserfs_gauge_t *gauge);

extern void libreiserfs_gauge_set_type(reiserfs_gauge_t *gauge, reiserfs_gauge_type_t type);
extern reiserfs_gauge_type_t libreiserfs_gauge_get_type(reiserfs_gauge_t *gauge);

extern void libreiserfs_gauge_set_data(reiserfs_gauge_t *gauge, void *data);
extern void* libreiserfs_gauge_get_data(reiserfs_gauge_t *gauge);

extern void libreiserfs_gauge_touch(reiserfs_gauge_t *gauge);

extern void libreiserfs_gauge_set_name(reiserfs_gauge_t *gauge, const char *name);
extern char *libreiserfs_gauge_get_name(reiserfs_gauge_t *gauge);

extern void libreiserfs_gauge_set_value(reiserfs_gauge_t *gauge, unsigned int value);
extern unsigned int libreiserfs_gauge_get_value(reiserfs_gauge_t *gauge);

extern void libreiserfs_gauge_set_state(reiserfs_gauge_t *gauge, reiserfs_gauge_state_t state);
extern reiserfs_gauge_state_t libreiserfs_gauge_get_state(reiserfs_gauge_t *gauge);

extern void libreiserfs_gauge_reset(reiserfs_gauge_t *gauge);
extern void libreiserfs_gauge_finish(reiserfs_gauge_t *gauge, int success);

#endif

