#include<linux/kobject.h>
#include<linux/string.h>
#include<linux/sysfs.h>
#include<linux/module.h>
#include<linux/init.h>
#include <linux/gpio.h>

/*
 * This module shows how to create a simple subdirectory in sysfs called
 * /sys/kernel/kobject-example  In that directory, a file is created:
 */

#define LED_GPIO    49
static int led_state;
/*
 * The "state" file where a static variable is read from and written to.
 */

static ssize_t led_state_show(struct kobject *kobj, struct kobj_attribute *attr, char*buf){
    return sprintf(buf,"%d\n", led_state);
}

static ssize_t led_state_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count){
    int ret;

    ret = kstrtoint(buf, 10, &led_state);
    if(ret < 0)
        return ret;

    if(led_state == 0)
        gpio_set_value(LED_GPIO,0);
    else if(led_state != 0)
        gpio_set_value(LED_GPIO,1);

    return count;
}

/* Sysfs attributes cannot be world-writable. */
static struct kobj_attribute state_attribute =
    __ATTR(led_state,0664,led_state_show,led_state_store);

static struct attribute *attrs[] = {
    &state_attribute.attr,
    NULL,/* need to NULL terminate the list of attributes */
};

/*
 * An unnamed attribute group will put all of the attributes directly in
 * the kobject directory.  If we specify a name, a subdirectory will be
 * created for the attributes with the directory being the name of the
 * attribute group.
 */
static struct attribute_group attr_group = {
    .name = "my_attr",
    .attrs = attrs,
};

static struct kobject *led_kobj;

static int __init led_init(void){
    int retval;
    
	bool valid = gpio_is_valid(LED_GPIO);
    if(!valid) {
        printk(KERN_ERR "%s: GPIO pin %d doesn't exist.\n", THIS_MODULE->name, LED_GPIO);
        retval = -1;
        goto gpio_invalid;
    }
    printk(KERN_INFO "%s: GPIO pin %d exists.\n", THIS_MODULE->name, LED_GPIO);

    retval = gpio_request(LED_GPIO, "bbb-led");
    if(retval != 0) {
        printk(KERN_ERR "%s: GPIO pin %d is busy.\n", THIS_MODULE->name, LED_GPIO);
        goto gpio_invalid;
    }
    printk(KERN_INFO "%s: GPIO pin %d acquired.\n", THIS_MODULE->name, LED_GPIO);

    led_state = 1;
    retval = gpio_direction_output(LED_GPIO, led_state);
    if(retval != 0) {
        printk(KERN_ERR "%s: GPIO pin %d direction not set.\n", THIS_MODULE->name, LED_GPIO);
        goto gpio_direction_failed;
    }
    printk(KERN_INFO "%s: GPIO pin %d direction set to OUTPUT.\n", THIS_MODULE->name, LED_GPIO);


    led_kobj = kobject_create_and_add("kobject_led", kernel_kobj);
    if(!led_kobj)
        return -ENOMEM;
    
    /* Create the files associated with this kobject */
    retval = sysfs_create_group(led_kobj,&attr_group);
    if(retval != 0)
        kobject_put(led_kobj);
    
    return retval;

gpio_direction_failed:
    gpio_free(LED_GPIO);
gpio_invalid:
    return retval;
}

static void __exit led_exit(void){
    kobject_put(led_kobj);
    gpio_free(LED_GPIO);
    printk(KERN_INFO "%s: GPIO pin %d released.\n", THIS_MODULE->name, LED_GPIO);
}

module_init(led_init);
module_exit(led_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("PARTH");
MODULE_DESCRIPTION("Simplified kobject demo from Greg Kroah-Hartman <greg@kroah.com>");