/**
 * File name: eca30.c
 * Project name: eca-lkm, elementary cellular automata implemented as a Linux kernel module
 * URL: https://github.com/ciubotaru/eca-lkm
 * Author: Vitalie Ciubotaru <vitalie at ciubotaru dot tk>
 * Date: July 10, 2017
 * License: General Public License, version 3 or later
 * Copyright 2017
**/

#include <linux/init.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/random.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <asm/uaccess.h>
#define DEVICE_NAME "eca30"
#define CLASS_NAME "eca"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Vitalie Ciubotaru");
MODULE_DESCRIPTION("RNG based on Wolfram's Rule 30 for elementary cellular automata");
MODULE_VERSION("0.0.1");

static int majorNumber;
static struct class* ecaClass = NULL;
static struct device* ecaDevice = NULL;

#define POOL_SIZE_BITS 257
#define POOL_SIZE_BYTES ((POOL_SIZE_BITS - 1) / sizeof(unsigned long) + 1)
#define POOL_SIZE_LONGS ((POOL_SIZE_BITS - 1) / BITS_PER_LONG + 1)
#define RULE30_BLOCK_SIZE 64
#define ENTRY_BIT (POOL_SIZE_BITS / 2)
#define ENTRY_LONG ((ENTRY_BIT - 1) / (sizeof(unsigned long) * 8) + 1)

static unsigned long pool[POOL_SIZE_LONGS] = {0};
static unsigned long r[POOL_SIZE_LONGS] = {0};
static unsigned long l[POOL_SIZE_LONGS] = {0};
static unsigned long o[POOL_SIZE_LONGS] = {0};
static __u8 tmp[RULE30_BLOCK_SIZE] = {0};

inline static unsigned char pool_valid(void) {
	int i;
	for (i = 0; i < POOL_SIZE_LONGS; i++) if (pool[i]) return 1;
	return 0;
}

inline static void right(void) {
	int i;
	for (i = 0; i < POOL_SIZE_LONGS; i++) r[i] = pool[i] >> 1;
	for (i = 1; i < POOL_SIZE_LONGS; i++) r[i-1] |= (pool[i] & 1UL) << (BITS_PER_LONG - 1);
	r[POOL_SIZE_LONGS - 1] |= (pool[0] & 1UL) << ((POOL_SIZE_BITS - 1) % BITS_PER_LONG);
}

inline static void left(void) {
	int i;
	for (i = 0; i < POOL_SIZE_LONGS; i++) l[i] = (pool[i] << 1);
	for (i = 1; i < POOL_SIZE_LONGS; i++) l[i] |= (pool[i - 1] >> (BITS_PER_LONG - 1)) & 1UL;
	l[0] |= (pool[POOL_SIZE_LONGS-1] >> ((POOL_SIZE_BITS - 1) % BITS_PER_LONG)) & 1UL;
}

static void rule30(void) {
	int i;
	right();
	left();
/**
	__bitmap_shift_right(r, pool, 1, POOL_SIZE_BITS);
	__bitmap_shift_left(l, pool, 1, POOL_SIZE_BITS);
**/
	for (i = 0; i < POOL_SIZE_LONGS; i++) {
		o[i] = pool[i] | r[i];
		pool[i] = l[i] ^ o[i];
	}
}

static void get_rand(unsigned char *buffer, const size_t len) {
	int i;
	if (pool_valid() != 1) {
		printk(KERN_INFO "eca30: Pool drained. Refilling...\n");
		get_random_bytes(pool, POOL_SIZE_BYTES);
	}
	for (i = 0; i < len * 8; i++) {
		/** requires a zeroed out buffer, so no 'else' **/
		if (pool[0] & 1UL) buffer[i/8] |= (pool[0] & 1UL) << (i % 8);
		rule30();
	}
}

static ssize_t dev_read(struct file *, char *, size_t, loff_t *);
static ssize_t dev_write(struct file *, const char *, size_t, loff_t *);

static struct file_operations fops =
{
	.read = dev_read,
	.write = dev_write,
};

static int __init eca30_init(void){
//	int i;
	printk(KERN_INFO "eca30: Initializing...\n");
	majorNumber = register_chrdev(0, DEVICE_NAME, &fops);
	if (majorNumber < 0) {
		printk(KERN_ALERT "eca30 failed to register a major number\n");
		return majorNumber;
	}
	printk(KERN_INFO "eca30: registered correctly with major number %d\n", majorNumber);

	ecaClass = class_create(THIS_MODULE, CLASS_NAME);
	if (IS_ERR(ecaClass)) {
		unregister_chrdev(majorNumber, DEVICE_NAME);
		printk(KERN_ALERT "Failed to register device class\n");
		return PTR_ERR(ecaClass);
	}
	printk(KERN_INFO "eca30: device class registered correctly\n");

	ecaDevice = device_create(ecaClass, NULL, MKDEV(majorNumber, 0), NULL, DEVICE_NAME);
	if (IS_ERR(ecaDevice)) {
		class_destroy(ecaClass);
		unregister_chrdev(majorNumber, DEVICE_NAME);
		printk(KERN_ALERT "Failed to create the device\n");
		return PTR_ERR(ecaDevice);
	}
	printk(KERN_INFO "eca30: device created correctly\n");

//	pool[0] = 1UL;
//	for (i = 0; i < POOL_SIZE_BITS; i++) rule30();
	return 0;
}

static void __exit eca30_exit(void) {
	device_destroy(ecaClass, MKDEV(majorNumber, 0));
	class_unregister(ecaClass);
   class_destroy(ecaClass);
   unregister_chrdev(majorNumber, DEVICE_NAME);
   printk(KERN_INFO "eca30: Unloading...\n");
}

static ssize_t dev_read(struct file *filep, char *buffer, size_t len, loff_t *offset) {
	ssize_t ret = 0, i = RULE30_BLOCK_SIZE, j = 0;

	while (len) {
//		get_rand(tmp, RULE30_BLOCK_SIZE);
		i = (len < RULE30_BLOCK_SIZE) ? len : RULE30_BLOCK_SIZE;
		get_rand(tmp, i);
		if (copy_to_user(buffer, tmp, i)) {
			ret = -EFAULT;
			break;
		}
		len -= i;
		buffer += i;
		ret += i;
		for (j = 0; j < i; j++) tmp[j] = 0;
	}
	return ret;
}

static ssize_t dev_write(struct file *filep, const char *buffer, size_t len, loff_t *offset) {
	int i;
	ssize_t ret = 0;
	while (len) {
		for (i = 0; i < 8; i++) {
			if (buffer[ret] >> i & 1) pool[ENTRY_LONG] |= (unsigned int) buffer[ret] >> i & 1UL;
			else pool[ENTRY_LONG] &= ~((unsigned int) buffer[ret] >> i & 1UL);
			rule30();
		}
		ret++;
		len--;
	}
	return ret;
}

module_init(eca30_init);
module_exit(eca30_exit);

