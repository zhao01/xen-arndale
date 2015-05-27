/* 
 * daytime.c: a simple network service based on lwIP and mini-os
 * 
 * Tim Deegan <Tim.Deegan@eu.citrix.net>, July 2007
 */

#include <os.h>
#include <mini-os/os.h>
#include <mini-os/hypervisor.h>
#include <mini-os/mm.h>
#include <mini-os/events.h>
#include <mini-os/time.h>
#include <xmalloc.h>
#include <console.h>
#include <netfront.h>
#include <lwip/api.h>
#include <mini-os/gnttab.h>
#include <mini-os/blkfront.h>
#include <mini-os/types.h>
#include <mini-os/xmalloc.h>
#include <fcntl.h>
#include <mini-os/lib.h>
#include <xen/version.h>
#include <xen/features.h>

#define BLKTEST_WRITE
static char message[29];
//static unsigned int do_shutdown = 0;

void run_server(void *p)
{
    struct ip_addr listenaddr = { 0 };
    struct netconn *listener;
    struct netconn *session;
    struct timeval tv;
    err_t rc;

    start_networking();

    if (0) {
        struct ip_addr ipaddr = { htonl(0x0a000001) };
        struct ip_addr netmask = { htonl(0xff000000) };
        struct ip_addr gw = { 0 };
        networking_set_addr(&ipaddr, &netmask, &gw);
    }

    tprintk("Opening connection\n");

    listener = netconn_new(NETCONN_TCP);
    tprintk("Connection at %p\n", listener);

    rc = netconn_bind(listener, &listenaddr, 13);
    if (rc != ERR_OK) {
        tprintk("Failed to bind connection: %i\n", rc);
        return;
    }

    rc = netconn_listen(listener);
    if (rc != ERR_OK) {
        tprintk("Failed to listen on connection: %i\n", rc);
        return;
    }

    while (1) {
        tprintk("yytang waiting for connecting\n");
        session = netconn_accept(listener);
        if (session == NULL) 
            continue;
        tprintk("yytang: connected to a new client\n");
        gettimeofday(&tv, NULL);
        sprintf(message, "%20lu.%6.6lu\n", tv.tv_sec, tv.tv_usec);
        tprintk("yytang: message is : %s\n", message);
        (void) netconn_write(session, message, strlen(message), NETCONN_COPY);
        (void) netconn_disconnect(session);
        (void) netconn_delete(session);
    }
}

#ifdef CONFIG_BLKFRONT
static struct blkfront_dev *blk_dev;
static struct blkfront_info blk_info;
static uint64_t blk_size_read;
static uint64_t blk_size_write;
static struct semaphore blk_sem = __SEMAPHORE_INITIALIZER(blk_sem, 0);;

struct blk_req {
    struct blkfront_aiocb aiocb;
    int rand_value;
    struct blk_req *next;
};

#ifdef BLKTEST_WRITE
static struct blk_req *blk_to_read;
#endif

static struct blk_req *blk_alloc_req(uint64_t sector)
{
    struct blk_req *req = xmalloc(struct blk_req);
    req->aiocb.aio_dev = blk_dev;
    req->aiocb.aio_buf = _xmalloc(blk_info.sector_size, blk_info.sector_size);
    req->aiocb.aio_nbytes = blk_info.sector_size;
    req->aiocb.aio_offset = sector * blk_info.sector_size;
    req->aiocb.data = req;
    req->next = NULL;
    return req;
}

static void blk_read_completed(struct blkfront_aiocb *aiocb, int ret)
{
    struct blk_req *req = aiocb->data;
    if (ret)
        printk("got error code %d when reading at offset %ld\n", ret, (long) aiocb->aio_offset);
    else
        blk_size_read += blk_info.sector_size;
    free(aiocb->aio_buf);
    free(req);
}

static void blk_read_sector(uint64_t sector)
{
    struct blk_req *req;

    req = blk_alloc_req(sector);
    req->aiocb.aio_cb = blk_read_completed;

    blkfront_aio_read(&req->aiocb);
}

#ifdef BLKTEST_WRITE
/*
static void blk_write_read_completed(struct blkfront_aiocb *aiocb, int ret)
{
    struct blk_req *req = aiocb->data;
    int rand_value;
    int i;
    int *buf;

    if (ret) {
        printk("got error code %d when reading back at offset %ld\n", ret, (long int)aiocb->aio_offset);
        free(aiocb->aio_buf);
        free(req);
        return;
    }
    blk_size_read += blk_info.sector_size;
    buf = (int*) aiocb->aio_buf;
    rand_value = req->rand_value;
    for (i = 0; i < blk_info.sector_size / sizeof(int); i++) {
        if (buf[i] != rand_value) {
            printk("bogus data at offset %ld, buf[%d] = %d, rand_value = %d\n", (long int)aiocb->aio_offset + i, i, buf[i], rand_value);
            break;
        }
        rand_value *= RAND_MIX;
    }
    free(aiocb->aio_buf);
    free(req);
}
*/
static void blk_write_completed(struct blkfront_aiocb *aiocb, int ret)
{
    struct blk_req *req = aiocb->data;
    if (ret) {
        printk("got error code %d when writing at offset %ld\n", ret, (long int)aiocb->aio_offset);
        free(aiocb->aio_buf);
        free(req);
        return;
    }
    blk_size_write += blk_info.sector_size;
    /* Push write check */
    req->next = blk_to_read;
    blk_to_read = req;
}

static void blk_write_sector(uint64_t sector)
{
    struct blk_req *req;
    int rand_value;
    int i;
    int *buf;

    req = blk_alloc_req(sector);
    req->aiocb.aio_cb = blk_write_completed;
    req->rand_value = rand_value = 0; //rand();

    buf = (int*) req->aiocb.aio_buf;
    for (i = 0; i < blk_info.sector_size / sizeof(int); i++) {
        buf[i] = rand_value;
        //rand_value *= RAND_MIX;
        rand_value ++; 
    }
    //printk("yytang: offset = %ld, rand_value = %d\n", (long)req->aiocb.aio_offset, rand_value);
    blkfront_aio_write(&req->aiocb);
}
#endif

static void blkfront_thread(void *p)
{
    blk_dev = init_blkfront(NULL, &blk_info);
    if (!blk_dev) {
        up(&blk_sem);
        return;
    }

    if (blk_info.info & VDISK_CDROM)
        printk("Block device is a CDROM\n");
    if (blk_info.info & VDISK_REMOVABLE)
        printk("Block device is removable\n");
    if (blk_info.info & VDISK_READONLY)
        printk("Block device is read-only\n");

#ifdef BLKTEST_WRITE
    if (blk_info.mode == O_RDWR) {
        blk_write_sector(0);
        blk_write_sector(blk_info.sectors-1);
    } else
    {
        blk_read_sector(0);
        blk_read_sector(blk_info.sectors-1);
    }
#endif
    //yytang
    return ;
}
#endif


/* Should be random enough for our uses */
int rand(void)
{
    static unsigned int previous;
    struct timeval tv;
    gettimeofday(&tv, NULL);
    previous += tv.tv_sec + tv.tv_usec;
    previous *= RAND_MIX;
    return previous;
}

int app_main(start_info_t *si)
{
    create_thread("server", run_server, NULL);
    create_thread("blkfront", blkfront_thread, si);
    
    return 0;
}
