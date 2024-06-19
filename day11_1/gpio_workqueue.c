#include<linux/module.h>
#include<linux/fs.h>
#include<linux/cdev.h>
#include<linux/device.h>
#include<linux/init.h>
#include<linux/uaccess.h>
#include<linux/gpio.h>
#include<linux/interrupt.h>
#include<linux/workqueue.h>
#include<linux/delay.h>

static int bbb_gpio_open(struct inode *, struct file *);
static int bbb_gpio_close(struct inode *, struct file *);
static ssize_t bbb_gpio_read(struct file *, char *, size_t, loff_t *);
static ssize_t bbb_gpio_write(struct file *, const char *, size_t, loff_t *);

#define LED_GPIO 49
#define SWITCH_GPIO 115

static struct work_struct work_qu;
static int led_state;
static dev_t devno;
static int major;
static struct class *pclass;
static struct cdev cdev;
static int irq;

static struct file_operations bbb_gpio_fops = {
	.owner = THIS_MODULE,
	.open = bbb_gpio_open,
	.release = bbb_gpio_close,
	.read = bbb_gpio_read,
	.write = bbb_gpio_write
};

static void workqueue_handler(struct work_struct *param){
    int i;
	printk(KERN_INFO"%s : workqueue_handler is called\n", THIS_MODULE->name);
	for(i=0; i<10;i++){
		led_state = !led_state;
		gpio_set_value(LED_GPIO,led_state);
		msleep(1000);
	}
    printk(KERN_INFO"%s : workqueue_handler is completed\n", THIS_MODULE->name);
}

static irqreturn_t switch_isr(int irq, void *param){

	printk(KERN_INFO"%s : switch_isr() is called\n", THIS_MODULE->name);
	schedule_work(&work_qu);	
    return IRQ_HANDLED;
}

static __init int bbb_gpio_init(void){
	
	int ret,minor;
	struct device *pdevice;

	printk(KERN_INFO "%s : bbb_gpio_init() is called\n",THIS_MODULE->name);
	
	ret = alloc_chrdev_region(&devno,0,1,"bbb_gpio");
	if(ret < 0){
		printk(KERN_ERR "%s :alloc_chrdev_region failed\n", THIS_MODULE->name);
		goto alloc_chrdev_failed;
	}
	major = MAJOR(devno);
	minor = MINOR(devno);
	printk(KERN_INFO"%s : alloc_chrdev_region() is done. Device no. %d-%d\n", THIS_MODULE->name, major,minor);

	pclass = class_create(THIS_MODULE,"mygpio_class");
	if(IS_ERR(pclass)){
		printk(KERN_ERR"%s : class_created is failed\n", THIS_MODULE->name);
		ret = -1;
		goto class_create_failed;
	}
	printk(KERN_INFO"%s : class_create done\n", THIS_MODULE->name);

	pdevice = device_create(pclass, NULL, devno, NULL, "bbb_gpio%d", 0);
	if(IS_ERR(pdevice)){
		printk(KERN_ERR"%s : device_create() failed\n", THIS_MODULE->name);
		ret = -1;
		goto device_create_failed;
	}
	printk(KERN_INFO"%s : device_create() is done\n", THIS_MODULE->name);

	cdev_init(&cdev,&bbb_gpio_fops);
	ret = cdev_add(&cdev, devno, 1);
	if(ret != 0){
		printk(KERN_ERR"%s : cdev_add() is failed\n", THIS_MODULE->name);
		goto cdev_add_failed;
	}

	bool valid = gpio_is_valid(LED_GPIO);
	if(!valid){
		printk(KERN_ERR"%s : GPIO pin %d doesn't exit.\n",THIS_MODULE->name,LED_GPIO);
		ret = -1;
		goto gpio_invalid;
	}
	printk(KERN_INFO"%s : GPIO pin %d exits\n",THIS_MODULE->name, LED_GPIO);

	ret = gpio_request(LED_GPIO,"bbb_led");
	if(ret != 0){
		printk(KERN_INFO"%s : GPIO pin %d is busy\n", THIS_MODULE->name,LED_GPIO);
		goto gpio_invalid;
	}

	led_state=1;
	ret = gpio_direction_output(LED_GPIO,led_state);
	if(ret != 0){
		printk(KERN_ERR"%s : GPIO pin %d direction not set\n",THIS_MODULE->name,LED_GPIO);
		goto gpio_direction_failed;
	}
	printk(KERN_INFO"%s : GPIO pin %d direction set to output\n",THIS_MODULE->name,LED_GPIO);
	
	valid = gpio_is_valid(SWITCH_GPIO);
	if(!valid){
		printk(KERN_INFO"%s : GPIO pin %d dosen't exits\n", THIS_MODULE->name,SWITCH_GPIO);
		ret = -1;
		goto switch_gpio_invalid;
	}

	ret = gpio_request(SWITCH_GPIO,"bbb_switch");
	if(ret != 0){
		printk(KERN_INFO"%s : GPIO pin %d is busy\n", THIS_MODULE->name,SWITCH_GPIO);
		goto switch_gpio_invalid;
	}
	printk(KERN_INFO"%s : GPIO pin %d exits\n",THIS_MODULE->name, SWITCH_GPIO);

	ret = gpio_direction_input(SWITCH_GPIO);
	if(ret != 0){
		printk(KERN_ERR"%s : gpio pin %d direction can't set\n",THIS_MODULE->name,SWITCH_GPIO );
		goto switch_gpio_invalid;
	}
	printk(KERN_INFO"%s : GPIO pin %d direction set to input\n",THIS_MODULE->name,SWITCH_GPIO);

	irq = gpio_to_irq(SWITCH_GPIO);
	ret = request_irq(irq,switch_isr,IRQF_TRIGGER_RISING, "bbb_switch",NULL);
	if(ret != 0){
		printk(KERN_INFO"%s : GPIO pin %d  ISR registration  failed\n", THIS_MODULE->name,SWITCH_GPIO);
		goto switch_gpio_direction_failed;
	}
	printk(KERN_INFO"%s : GPIO pin %d registerd ISR on irq line %d\n",THIS_MODULE->name,SWITCH_GPIO,irq);

    //initialize workqueue
	INIT_WORK(&work_qu,workqueue_handler);

	return 0;

switch_gpio_direction_failed:
    gpio_free(SWITCH_GPIO);
switch_gpio_invalid:
gpio_direction_failed:
    gpio_free(LED_GPIO);
gpio_invalid:
    cdev_del(&cdev);
cdev_add_failed:
	device_destroy(pclass, devno);
device_create_failed:
	class_destroy(pclass);
class_create_failed:
	unregister_chrdev_region(devno, 1);
alloc_chrdev_failed:
	return ret;

}

static __exit void bbb_gpio_exit(void){
	
	printk(KERN_INFO"%s : bbb_gpio_exit() is called\n", THIS_MODULE->name);
    free_irq(irq, NULL);
    printk(KERN_INFO "%s: GPIO pin %d ISR released.\n", THIS_MODULE->name, SWITCH_GPIO);
	gpio_free(LED_GPIO);
	printk(KERN_INFO"%s : GPIO pin %d released\n", THIS_MODULE->name,LED_GPIO);
	gpio_free(SWITCH_GPIO);
	printk(KERN_INFO"%s : GPIO pin %d released\n", THIS_MODULE->name,SWITCH_GPIO);
	cdev_del(&cdev);
	printk(KERN_INFO" %s : cdev_del() is called\n", THIS_MODULE->name);
	device_destroy(pclass, devno);
	printk(KERN_INFO"%s : device_destroy is called\n", THIS_MODULE->name);
	class_destroy(pclass);
	printk(KERN_INFO "%s : class_destroy is called\n", THIS_MODULE->name);
	unregister_chrdev_region(devno, 1);
	printk(KERN_INFO "%s : unregister_chrdev_region is called\n",THIS_MODULE->name);
	
}

static int bbb_gpio_open(struct inode *pinode, struct file *pfile){
	printk(KERN_INFO"open is called\n");
	return 0;
}
static int bbb_gpio_close(struct inode *pinode, struct file *pfile){

	printk(KERN_INFO"close is called\n");
	return 0;
}

static ssize_t bbb_gpio_read(struct file *pfile, char *ubuf, size_t size, loff_t *poffset){
	int ret,switch_state;
	char kbuf[4];
	switch_state = gpio_get_value(SWITCH_GPIO);
	printk(KERN_INFO"%s : bbb_gpio_read is called\n", THIS_MODULE->name);
	sprintf(kbuf,"%d\n",led_state);
	ret = 2 - copy_to_user(ubuf,kbuf,2);
	printk(KERN_INFO"%s : GPIO pin %d LED state read = %d\n", THIS_MODULE->name,LED_GPIO,led_state);
	return size;
}

static ssize_t bbb_gpio_write(struct file *pfile, const char *ubuf, size_t size, loff_t *poffset){
	int ret;
	char kbuf[2]="";
	printk(KERN_INFO"%s : bbb_gpio_write is called\n",THIS_MODULE->name);
	ret = 1 - copy_from_user(kbuf,ubuf,1);
	if(ret > 0){
		if(kbuf[0] == '1'){
			led_state = 1;
			gpio_set_value(LED_GPIO,led_state);
		}
		else if(kbuf[0] == '0'){
			led_state = 0;
			gpio_set_value(LED_GPIO,led_state);
		}
		else{
			printk(KERN_INFO"%s : invalid argument\n", THIS_MODULE->name);
		}
	}
	printk(KERN_INFO"%s : bbb_gpio_write() closed\n", THIS_MODULE->name);
	return size;
}

module_init(bbb_gpio_init);
module_exit(bbb_gpio_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("PARTH");
MODULE_DESCRIPTION("This is BBB GPIO tasklet");