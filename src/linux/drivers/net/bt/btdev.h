#ifndef _BTDEV_H_
#define _BTDEV_H_

#include <linux/skbuff.h>
#include <linux/list.h>
#include <linux/errno.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/fcntl.h>
#include <linux/ioport.h>
#include <linux/in.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/string.h>
#include <linux/bitops.h>
#include <linux/delay.h>
#include <linux/ip.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/poll.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/ktime.h>
#include <linux/rtnetlink.h>

/* must include btdev_user.h first before any macro definition */
#include "btdev_user.h"

#define BT_DEV_MAJOR                125
#define BT_DEV_MINOR                  0
#define BT_RING_BUFFER_SIZE        4096
#define STRTOLL_BASE                 10
#define BT_DEV_ID_OFFSET           (sizeof(BT_DEV_PATH_PREFIX) - 1)
#define BT_STATISTIC_KTIME_MAX     ULONG_MAX

/*
 * for debug
 */
#define DEBUG

/**
 * ring buffer
 */
typedef struct bt_ring {
      u32     head;
      u32     tail;
      u32     size;
      void    **data;
} bt_ring_t;

/**
 * one char device
 */
typedef struct bt_cdev {
    struct cdev  *cdev;
    struct class *bt_class;
    char         dev_filename[BT_PATHNAME_MAX];
} bt_cdev_t;

typedef struct bt_mng_file {
    bt_cdev_t  *bt_cdev;
    atomic_t    open_limit;
} bt_mng_file_t;


typedef struct bt_io_file {
    bt_cdev_t  *bt_cdev;
    atomic_t    read_open_limit;
    atomic_t    write_open_limit;
} bt_io_file_t;

/**
 * virnet list
 */
typedef struct bt_table {
    struct list_head    head;
    struct mutex        tbl_lock;
    uint32_t            num;
} bt_table_t;

/**
 * bt virnet state
 */
enum bt_virnet_state {
    BT_VIRNET_STATE_CREATED,
    BT_VIRNET_STATE_CONNECTED,
    BT_VIRNET_STATE_DISCONNECTED,
    BT_VIRNET_STATE_DISABLED,
    BT_VIRNET_STATE_DELETED,
    BT_VIRNET_STAET_NUM
};

/**
 * one virnet device
 */
typedef struct bt_virnet {
    bt_ring_t               *tx_ring;
    bt_io_file_t            *io_file;
    struct net_device       *ndev;
    struct list_head        virnet_entry;
    bt_table_t              *bt_table_head;
    enum bt_virnet_state    state;
    struct semaphore        sem;
    wait_queue_head_t       rx_queue, tx_queue;
} bt_virnet_t;

/**
 * instance of the module
 */
typedef struct bt_drv {
    bt_table_t        *devices_table;
    bt_mng_file_t     *mng_file;
    bt_io_file_t     **io_files;
    uint32_t           bitmap;
    struct mutex       bitmap_lock;
    struct class      *bt_class;
} bt_drv_t;

/**
 * state to string
 */
static const char* bt_virnet_state_rep[BT_VIRNET_STAET_NUM] = {
    "CREATED",
    "CONNECTED",
    "DISCONNECTED",
    "DISABLED",
    "ENABLED"
};

/**
 * inline functions
 */
static inline int bt_get_unused_id(const uint32_t *bitmap)
{
    int i;
    BUG_ON(!bitmap);
    for (i=0; i<BT_VIRNET_MAX_NUM+1; ++i) {
        if (!(*bitmap & (1 << i))) return i;
    }
    return -1; // all used
}

static inline void bt_set_bit(uint32_t *bitmap, uint32_t idx)
{
    BUG_ON(!bitmap);
    *bitmap |= (1<<idx);
}

static inline void bt_clear_bit(uint32_t *bitmap, uint32_t idx)
{
    BUG_ON(!bitmap);
    *bitmap &= ~(1<<idx);
}

#define SET_STATE(vn, st)  bt_virnet_set_state(vn, st)
static inline void bt_virnet_set_state(bt_virnet_t *vn,
                                        enum bt_virnet_state state)
{
    BUG_ON(!vn);
    vn->state = state;
}

static inline const struct cdev *bt_virnet_get_cdev(const bt_virnet_t *vn)
{
    BUG_ON(!vn);
    return vn->io_file->bt_cdev->cdev;
}

static inline const dev_t bt_virnet_get_cdev_number(const bt_virnet_t *vn)
{
    BUG_ON(!vn);
    return vn->io_file->bt_cdev->cdev->dev;
}

static inline const char *bt_virnet_get_cdev_name(const bt_virnet_t *vn)
{
    BUG_ON(!vn);
    return vn->io_file->bt_cdev->dev_filename;
}

static inline struct net_device *bt_virnet_get_ndev(const bt_virnet_t *vn)
{
    BUG_ON(!vn);
    return vn->ndev;
}

static inline const char *bt_virnet_get_ndev_name(const bt_virnet_t *vn)
{
    BUG_ON(!vn);
    return vn->ndev->name;
}

static inline const char *bt_virnet_get_state_rep(const bt_virnet_t *vn)
{
    BUG_ON(!vn);
    return bt_virnet_state_rep[vn->state];
}

static inline int bt_get_total_device(const bt_drv_t *bt_drv)
{
    BUG_ON(!bt_drv);
    return bt_drv->devices_table->num;
}

static inline int bt_virnet_get_ring_packets(const bt_virnet_t *vn)
{
    int packets = 0;
    BUG_ON(!vn);
    packets = vn->tx_ring->head - vn->tx_ring->tail;
    if (unlikely(packets < 0)) {
        packets += BT_RING_BUFFER_SIZE;
    }
    return packets;
}

static bt_table_t *bt_table_init(void);
static int bt_table_add_device(bt_table_t *tbl, bt_virnet_t *vn);
static void bt_table_remove_device(bt_table_t *tbl, bt_virnet_t *vn);
static void bt_table_delete_all(bt_drv_t *drv);
static bt_virnet_t *bt_table_find(bt_table_t *tbl, const char *ifa_name);
static void bt_table_destroy(bt_drv_t *drv);
static void bt_delete_io_files(bt_drv_t *drv);
static bt_io_file_t **bt_create_io_files(void);

static bt_ring_t *bt_ring_create(void);
static int bt_ring_is_empty(const bt_ring_t *ring);
static int bt_ring_is_full(const bt_ring_t *ring);
static void *bt_ring_current(bt_ring_t *ring);
static void bt_ring_produce(bt_ring_t *ring, void *data);
static void bt_ring_consume(bt_ring_t *ring);
static void bt_ring_clear(bt_ring_t *ring);
static void bt_ring_destroy(bt_ring_t *ring);

static int bt_virnet_produce_data(bt_virnet_t *dev, void *data);
static bt_virnet_t *bt_virnet_create(bt_drv_t *drv, uint32_t id);
static void bt_virnet_destroy(bt_virnet_t *dev);

#endif
