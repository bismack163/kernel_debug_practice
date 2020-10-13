#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h> 
#include <linux/nmi.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/spi/spi.h>
#include <linux/interrupt.h>
#include <linux/irqnr.h>
#include <linux/kernel_stat.h>
#include <linux/pipe_fs_i.h>
#include <linux/file.h>
#include <linux/fs.h>
#include <asm/segment.h>
#include <asm/uaccess.h>

//#define TEST_IRQ1	451 //461
#define TEST_IRQ1	450 //461
//#define TEST_IRQ2	438 //GPIO_4
#define TEST_IRQ2	442 //GPIO_8
static pid_t pid;

module_param(pid, int, S_IRUGO | S_IWUSR);

struct umh_data {
	struct file *f_in;
	int started;
};

static struct umh_data pipedata;

static int umh_pipe_setup(struct subprocess_info *info, struct cred *new) {
	struct file *files[2];
	struct umh_data *cp = (struct umh_data *)info->data;
	pr_crit("%s pid=%d comm=%s cp=%p pipedata=%p\n", __func__, current->pid, current->comm, cp, &pipedata);
	int err = create_pipe_files(files, 0);
	if (err)
		return err;

	cp->f_in = files[0];
	cp->started = 1;

	pr_crit("files[0]=%p files[0] mode=%x\n", files[0], files[0]->f_mode);
	pr_crit("files[1]=%p files[1] mode=%x\n", files[1], files[1]->f_mode);
	pr_crit("%s %d f=%p fmode=%x pipedata.started=%d\n",
			__func__, __LINE__, pipedata.f_in, pipedata.f_in->f_mode, pipedata.started);
	err = replace_fd(1, files[1], 0);
	fput(files[1]);
	pr_crit("%s %d f=%p fmode=%x pipedata.started=%d\n",
			__func__, __LINE__, pipedata.f_in, pipedata.f_in->f_mode, pipedata.started);
	return 0;
}

//int file_read(struct file *file, unsigned char *data, size_t size, loff_t offset)
//{
//    mm_segment_t oldfs;
//    int ret;
//
//    oldfs = get_fs();
//    set_fs(get_ds());
//
//    ret = vfs_read(file, data, size, &offset);
//
//    set_fs(oldfs);
//    return ret;
//}

struct file *file_open(const char *path, int flags, int rights) {
	struct file *filp = NULL;
	mm_segment_t oldfs;
	int err = 0;

	oldfs = get_fs();
	set_fs(get_ds());
	filp = filp_open(path, flags, rights);
	set_fs(oldfs);
	if (IS_ERR(filp)) {
		err = PTR_ERR(filp);
		return NULL;
	}
	return filp;
}
//Close a file (similar to close):

void file_close(struct file *file) {
	filp_close(file, NULL);
}
//Reading data from a file (similar to pread):

int file_read(struct file *file, unsigned char *data,
		size_t size, loff_t offset) {
	mm_segment_t oldfs;
	int ret;

	oldfs = get_fs();
	set_fs(get_ds());

	ret = vfs_read(file, data, size, &offset);

	set_fs(oldfs);
	return ret;
}
//Writing data to a file (similar to pwrite):

int file_write(struct file *file, loff_t offset,
		unsigned char *data, size_t size) {
	mm_segment_t oldfs;
	int ret;

	oldfs = get_fs();
	set_fs(get_ds());

	ret = vfs_write(file, data, size, &offset);

	set_fs(oldfs);
	return ret;
}
//Syncing changes a file (similar to fsync):

int file_sync(struct file *file) {
	vfs_fsync(file, 0);
	return 0;
}

int read_from_file(char* filepath, char* buf, size_t size) {
	int ret = -1;
	struct file *f = file_open(filepath, O_RDONLY, O_RDONLY);
	if (!f) {
		return ret;
	}
	ret = file_read(f, buf, size, 0);
	file_close(f);
	return ret;
}

static int umh_test(void) {
#define CMD_OUT_BUF_SZ 1024
	struct subprocess_info *sub_info;
	static char topbuf[CMD_OUT_BUF_SZ];
	static char cmdbuf[1024];

	memset(cmdbuf, 0, 1024);
	int cmdlen = read_from_file("/data/cmd", cmdbuf, 1023);
	if (cmdlen <= 0)
		return 0;
	if (cmdbuf[cmdlen-1] == '\n')
		cmdbuf[cmdlen-1] = 0;
	else
		cmdbuf[cmdlen] = 0;
	pr_crit("cmd read %s\n", cmdbuf); //"/system/bin/top -b -n1"
//	static char *argv[] = { "/system/bin/ls", "/data", 0 };
	static char *argv[100];// = { "/system/bin/ls", "/data", 0 };
	argv[99] = 0;
	int i;
	char *pcmd = cmdbuf;
	for (i = 0; i < 99; i++) {
		argv[i] = strsep(&pcmd, " ");
		if (!argv[i]) {
			argv[i+1] = NULL;
			break;
		}
		pr_crit("argv[%d]=%s\n", i, argv[i]);
	}
	if (!argv[0])
		return 0;
//	static char *envp[] = { "USER=root", "_=/system/bin/ls", 0 };
	pipedata.started = 0;

	sub_info = call_usermodehelper_setup(argv[0], argv, NULL, GFP_KERNEL,
			umh_pipe_setup, NULL, &pipedata);
	if (sub_info == 0)
		return -ENOMEM;

	pr_crit("%s pid=%d comm=%s\n", __func__, current->pid, current->comm);
	pr_crit("%s %d f=%p pipedata.started=%d\n", __func__, __LINE__, pipedata.f_in, pipedata.started);
	int ret = call_usermodehelper_exec(sub_info, UMH_WAIT_EXEC);
//	pr_crit("%s %d f=%p fmode=%x pipedata.started=%d\n", __func__, __LINE__, pipedata.f_in, pipedata.f_in->f_mode, pipedata.started);
	pr_crit("%s ret=%d\n", __func__, ret);
	if (ret != 0)
		return ret;
//	pr_crit("%s %d f=%p fmode=%x pipedata.started=%d\n", __func__, __LINE__, pipedata.f_in, pipedata.f_in->f_mode, pipedata.started);
//	int n=0;
//	while (!pipedata.started && (n++ < 500)) {
//		msleep(1);
//	}
//	if (n >= 500) {
//		pr_crit("time out waiting for started\n");
//		return -ETIME;
//	}
	long long offset = 0;
	int err = replace_fd(0, pipedata.f_in, 0);
	int nread, accu = 0;
	topbuf[CMD_OUT_BUF_SZ-1] = 0;
	do {
		nread = file_read(pipedata.f_in, topbuf + accu, CMD_OUT_BUF_SZ - 1 -accu, offset);
//		pr_crit("nread=%d", nread);
		accu += nread;

//		if (nread > 0) topbuf[nread] = 0;
	} while (nread > 0 && accu < CMD_OUT_BUF_SZ - 1);
	topbuf[accu] = 0;
	pr_crit("total = %d", accu);
	char* pbuf = topbuf;
	while (pbuf) {

		pr_crit("%s\n", strsep(&pbuf, "\n"));
	}
	filp_close(pipedata.f_in, NULL);
	return 0;
}

extern struct irqaction chained_action;
static inline int debug_irq_desc_is_chained(struct irq_desc *desc)
{
	return (desc->action && desc->action == &chained_action);
}

static DEFINE_SPINLOCK(show_lock);
static void showacpu(void *dummy)
{
	unsigned long flags;
//
//	/* Idle CPUs have no interesting backtrace. */
//	if (idle_cpu(smp_processor_id()))
//		return;

	spin_lock_irqsave(&show_lock, flags);
	pr_info("CPU%d:\n", smp_processor_id());
	show_stack(NULL, NULL);
	spin_unlock_irqrestore(&show_lock, flags);
}

int debug_show_interrupts(int irq)
{
	static int prec;

	unsigned long flags, any_count = 0;
	int i = irq, j;
	struct irqaction *action;
	struct irq_desc *desc;
	static char buf[1024];
#define BUF_SIZE 1024
	int len = 0;

	if (i > nr_irqs)
		return 0;

	/* print header and calculate the width of the first column */
	if (i == 0) {
		for (prec = 3, j = 1000; prec < 10 && j <= nr_irqs; ++prec)
			j *= 10;

		len += snprintf(buf+len, BUF_SIZE-len, "%*s", prec + 8, "");
		for_each_online_cpu(j)
			len += snprintf(buf+len, BUF_SIZE-len, "CPU%-8d", j);
		len += snprintf(buf+len, BUF_SIZE-len, "\n");
	}

	irq_lock_sparse();
	desc = irq_to_desc(i);
	if (!desc)
		goto outsparse;

	raw_spin_lock_irqsave(&desc->lock, flags);
	for_each_online_cpu(j)
		any_count |= kstat_irqs_cpu(i, j);
	action = desc->action;
	if ((!action || debug_irq_desc_is_chained(desc)) && !any_count)
		goto out;

	len += snprintf(buf+len, BUF_SIZE-len, "%*d: ", prec, i);
	for_each_online_cpu(j)
		len += snprintf(buf+len, BUF_SIZE-len, "%10u ", kstat_irqs_cpu(i, j));

	if (desc->irq_data.chip) {
		if (desc->irq_data.chip->name)
			len += snprintf(buf+len, BUF_SIZE-len, " %8s", desc->irq_data.chip->name);
		else
			len += snprintf(buf+len, BUF_SIZE-len, " %8s", "-");
	} else {
		len += snprintf(buf+len, BUF_SIZE-len, " %8s", "None");
	}
	if (desc->irq_data.domain)
		len += snprintf(buf+len, BUF_SIZE-len, " %*d", prec, (int) desc->irq_data.hwirq);
#ifdef CONFIG_GENERIC_IRQ_SHOW_LEVEL
	len += snprintf(buf+len, BUF_SIZE-len, " %-8s", irqd_is_level_type(&desc->irq_data) ? "Level" : "Edge");
#endif
	if (desc->name)
		len += snprintf(buf+len, BUF_SIZE-len, "-%-8s", desc->name);

	if (action) {
		len += snprintf(buf+len, BUF_SIZE-len, "  %s", action->name);
		while ((action = action->next) != NULL)
			len += snprintf(buf+len, BUF_SIZE-len, ", %s", action->name);
	}

	len += snprintf(buf+len, BUF_SIZE-len, "\n");
	pr_crit("%s", buf);
out:
	raw_spin_unlock_irqrestore(&desc->lock, flags);
outsparse:
	irq_unlock_sparse();
	return 0;
}

static irqreturn_t test_irq_thread_fn(int irq, void *_test) {
	pr_crit("start top cmd\n");
	umh_test();
	return IRQ_HANDLED;
}
static irqreturn_t test_irq_handler(int irq, void *_test)
{
	pr_crit("TEST irq work %d target pid=%d \n", irq, pid);
	int i;
	for (i = 0; i < nr_irqs; i ++) {
		debug_show_interrupts(i);
	}
//	trigger_all_cpu_backtrace();
	smp_call_function_many(cpu_online_mask, showacpu, NULL, 0);
	return IRQ_WAKE_THREAD;
}


static int __init test_init(void)
{
	int ret;

	printk("[TEST] LGE test driver probe\n");
	printk("[TEST] GPIO request 451 (GPIO17)\n");

/*
	ret = gpio_request(TEST_IRQ1, "test irq1");
	if (ret < 0) {
		printk("[%s]Failed to request irq for test\n", __func__);
		return ret;
	}
	else {
			printk("[%s]Successfully do GPIO request for 451\n", __func__);
	}
*/
	ret = request_threaded_irq(gpio_to_irq(TEST_IRQ1), test_irq_handler, test_irq_thread_fn,
					IRQF_TRIGGER_FALLING | IRQF_SHARED, "test_irq1", (void *)1);
	if (ret < 0) {
		printk("[%s]Failed to request irq for test %d\n", __func__, ret);
	}
	else {
		printk("[%s]Successfully requested irq for %d\n", __func__, TEST_IRQ1);
	}

#if 0
	printk("[TEST] request 335 (GPIO_155 ISH_GPIO_9)\n");
	ret = request_irq(gpio_to_irq(335), test_irq_handler, 
					IRQF_TRIGGER_FALLING | IRQF_SHARED, "test_irq2", (void *)1);
	if (ret < 0) {
		printk("[%s]Failed to request irq for test\n", __func__);
	}
#endif

	printk("[TEST] request 438\n");
	ret = gpio_request(TEST_IRQ2, "test irq2");
	gpio_direction_input(TEST_IRQ2);

	ret = request_irq(gpio_to_irq(438), test_irq_handler, 
			IRQF_TRIGGER_FALLING | IRQF_SHARED, "test_irq2", (void *)1);
	if (ret < 0) {
		printk("[%s]Failed to request irq for test\n", __func__);
	}
	else {
		printk("[%s]Successfully requested irq for 438\n", __func__);
	}


	return 0;

#if 0
	ret = gpio_request(MICOM_INT_8, "test-int-8");
	if (ret < 0) {
		printk("[%s]Failed to request gpio %d\n", __func__, MICOM_INT_8);
		return ret;
	}
#endif

	return 0;
}

static void __exit test_exit(void)
{
	printk("Un-registering LGE TEST driver.\n");
}

module_init(test_init);
module_exit(test_exit);

MODULE_LICENSE("GPL v2");
