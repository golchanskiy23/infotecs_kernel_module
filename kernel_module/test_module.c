#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/timer.h>
#include <linux/workqueue.h>
#include <linux/fs.h>
#include <linux/file.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/version.h>
#include <linux/stat.h>
#include <linux/atomic.h>
#include <linux/limits.h>
#include <linux/types.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Golchanskiy Maxim");
MODULE_DESCRIPTION("Kernel module that writes messages to a file periodically");

#define MAX_PATH_LEN (PATH_MAX - 1)
#define MAX_PERIOD 3600
#define MIN_PERIOD 1

static char *filename = "/var/tmp/test_module/kernel_log.txt";
module_param(filename, charp, 0644);
MODULE_PARM_DESC(filename, "Path to the log file");

static unsigned int timer_period = 5;
module_param(timer_period, uint, 0644);
MODULE_PARM_DESC(timer_period, "Timer period in seconds (1-3600)");

struct write_work {
    struct work_struct work;
    char *message;
    char *filepath;
};

struct test_module_state {
    struct timer_list write_timer;
    struct workqueue_struct *wq;
    atomic_t write_counter;
    bool module_active;
};

static struct test_module_state *module_state = NULL;

static bool is_valid_path(const char *path)
{
    size_t len;

    if (!path){
        return false;
    }

    len = strnlen(path, MAX_PATH_LEN + 1);
    if (len == 0 || len > MAX_PATH_LEN){
        return false;
    }

    return true;
}

static int write_to_file(const char *message, const char *filepath)
{
    struct file *filp;
    loff_t pos;
    int ret = 0;
    char *file_path = NULL;
    ssize_t written;
    size_t msg_len;

    if (!message) {
        pr_err("test_module: NULL message pointer\n");
        return -EINVAL;
    }

    if (!is_valid_path(filepath)) {
        pr_err("test_module: Invalid file path (NULL or too long)\n");
        return -EINVAL;
    }

    msg_len = strlen(message);
    if (msg_len == 0) {
        pr_warn("test_module: Empty message, skipping write\n");
        return 0;
    }

    file_path = kstrdup(filepath, GFP_KERNEL);
    if (!file_path) {
        pr_err("test_module: Failed to allocate memory for file path\n");
        return -ENOMEM;
    }

    filp = filp_open(file_path, O_RDWR | O_CREAT | O_APPEND, 0644);
    if (IS_ERR(filp)) {
        ret = PTR_ERR(filp);
        pr_err("test_module: Failed to open file %s, error: %d (%s)\n", 
               file_path, ret, 
               ret == -ENOENT ? "ENOENT - check directory exists and permissions" :
               ret == -EACCES ? "EACCES - permission denied" :
               ret == -ENOSPC ? "ENOSPC - no space left" : "unknown error");
        goto out_free_path;
    }

    pos = i_size_read(file_inode(filp));

    written = kernel_write(filp, message, msg_len, &pos);
    if (written < 0) {
        ret = (int)written;
        pr_err("test_module: Failed to write to file, error: %d\n", ret);
    } else if ((size_t)written != msg_len) {
        pr_warn("test_module: Partial write: %zd of %zu bytes\n", written, msg_len);
        ret = -EIO;
    }

    filp_close(filp, NULL);

out_free_path:
    kfree(file_path);
    return ret;
}

static void write_work_handler(struct work_struct *work)
{
    struct write_work *w = container_of(work, struct write_work, work);
    int ret;

    if (!w->message || !w->filepath) {
        pr_err("test_module: Invalid work parameters\n");
        goto out;
    }

    ret = write_to_file(w->message, w->filepath);
    if (ret < 0) {
        pr_err("test_module: Failed to write message to file (error: %d)\n", ret);
    }

out:
    kfree(w->message);
    kfree(w->filepath);
    kfree(w);
}

static void timer_callback(struct timer_list *t)
{
    struct test_module_state *state;
    struct write_work *w;
    char *message = NULL;
    char *filepath = NULL;
    int len;
    unsigned int counter;
    unsigned long delay;

    state = container_of(t, struct test_module_state, write_timer);

    if (!state || !module_state || state != module_state) {
        pr_warn("test_module: Timer callback called with invalid state\n");
        return;
    }

    if (!state->module_active) {
        pr_warn("test_module: Timer callback called after module deactivation\n");
        return;
    }

    counter = atomic_inc_return(&state->write_counter);

    if (counter == 0) {
        atomic_set(&state->write_counter, 1);
        counter = 1;
    }

    len = snprintf(NULL, 0, "Hello from kernel module (%u)\n", counter);
    if (len < 0) {
        pr_err("test_module: snprintf failed\n");
        goto reschedule;
    }

    w = kmalloc(sizeof(*w), GFP_ATOMIC);
    if (!w) {
        pr_err("test_module: Failed to allocate memory for work\n");
        goto reschedule;
    }

    message = kmalloc(len + 1, GFP_ATOMIC);
    if (!message) {
        pr_err("test_module: Failed to allocate memory for message\n");
        kfree(w);
        goto reschedule;
    }

    if (!filename) {
        pr_err("test_module: Filename parameter is NULL\n");
        kfree(message);
        kfree(w);
        goto reschedule;
    }

    filepath = kstrdup(filename, GFP_ATOMIC);
    if (!filepath) {
        pr_err("test_module: Failed to allocate memory for filepath\n");
        kfree(message);
        kfree(w);
        goto reschedule;
    }

    len = snprintf(message, len + 1, "Hello from kernel module (%u)\n", counter);
    if (len < 0) {
        pr_err("test_module: snprintf failed when formatting message\n");
        kfree(message);
        kfree(filepath);
        kfree(w);
        goto reschedule;
    }

    INIT_WORK(&w->work, write_work_handler);
    w->message = message;
    w->filepath = filepath;

    if (state->wq && state->module_active) {
        queue_work(state->wq, &w->work);
    } else {
        pr_err("test_module: Workqueue not initialized or module inactive\n");
        kfree(message);
        kfree(filepath);
        kfree(w);
        goto reschedule;
    }

reschedule:
    /* Проверяем module_active еще раз перед перепланированием таймера */
    if (state && state->module_active && timer_period > 0) {
        delay = msecs_to_jiffies(timer_period * 1000);
        if (delay == 0)
            delay = 1;
        mod_timer(&state->write_timer, jiffies + delay);
    }
}

static int __init test_module_init(void)
{
    unsigned long delay;

    pr_info("test_module: Initializing module\n");
    pr_info("test_module: Filename: %s\n", filename ? filename : "(NULL)");
    pr_info("test_module: Timer period: %u seconds\n", timer_period);

    if (!filename || !is_valid_path(filename)) {
        pr_err("test_module: Invalid filename parameter\n");
        return -EINVAL;
    }

    if (timer_period < MIN_PERIOD || timer_period > MAX_PERIOD) {
        pr_err("test_module: Timer period must be between %u and %u seconds\n",
               MIN_PERIOD, MAX_PERIOD);
        return -EINVAL;
    }

    module_state = kmalloc(sizeof(*module_state), GFP_KERNEL);
    if (!module_state) {
        pr_err("test_module: Failed to allocate memory for module state\n");
        return -ENOMEM;
    }

    atomic_set(&module_state->write_counter, 0);
    module_state->module_active = false;

    module_state->wq = alloc_workqueue("test_module_wq", WQ_MEM_RECLAIM, 1);
    if (!module_state->wq) {
        pr_err("test_module: Failed to create workqueue\n");
        kfree(module_state);
        return -ENOMEM;
    }

    timer_setup(&module_state->write_timer, timer_callback, 0);
    
    delay = msecs_to_jiffies(timer_period * 1000);
    if (delay == 0)
        delay = 1;

    module_state->module_active = true;
    
    mod_timer(&module_state->write_timer, jiffies + delay);

    pr_info("test_module: Module initialized successfully\n");
    return 0;
}

static void __exit test_module_exit(void)
{
    unsigned int total_writes = 0;

    pr_info("test_module: Removing module\n");

    if (!module_state) {
        pr_warn("test_module: Module state is NULL during exit\n");
        return;
    }

    total_writes = atomic_read(&module_state->write_counter);

    module_state->module_active = false;

    if (timer_pending(&module_state->write_timer)) {
        timer_delete_sync(&module_state->write_timer);
    }

    if (module_state->wq) {
        flush_workqueue(module_state->wq);
        destroy_workqueue(module_state->wq);
        module_state->wq = NULL;
    }

    /* Записываем финальное сообщение только если filename валиден */
    if (filename && is_valid_path(filename)) {
        write_to_file("Module unloaded\n", filename);
    }

    kfree(module_state);
    module_state = NULL;

    pr_info("test_module: Module removed (total writes: %u)\n", total_writes);
}

module_init(test_module_init);
module_exit(test_module_exit);