#include <linux/module.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/kfifo.h>
#include <linux/slab.h>
#include "pchar_ioctl.h"

static int pchar_open(struct inode *pinode, struct file *pfile);
static int pchar_close(struct inode *pinode, struct file *pfile);
static ssize_t pchar_read(struct file *pfile, char *ubuf, size_t size, loff_t *poffset);
static ssize_t pchar_write(struct file *pfile, const char *ubuf, size_t size, loff_t *poffset);
static long pchar_ioctl(struct file *pfile, unsigned int cmd, unsigned long param);

#define MAX 32

// device private struct
struct pchar_device
{
    struct kfifo my_buf;
    dev_t my_devno;
    struct cdev my_cdev;    
};

struct file_operations my_fops = {
    .owner = THIS_MODULE,
    .open = pchar_open,
    .release = pchar_close,
    .read = pchar_read,
    .write = pchar_write,
    .unlocked_ioctl = pchar_ioctl
};


static int major;
static struct class *pclass;
static int my_devcnt = 3;
module_param(my_devcnt,int,0100);
struct pchar_device *my_devices;

static __init int pchar_init(void)
{
    dev_t devno;
    int ret, i,minor;
    struct device *pdevices;

    printk(KERN_INFO "%s : pchar_init called\n", THIS_MODULE->name);

    my_devices = (struct pchar_device *)kmalloc(my_devcnt * sizeof(struct pchar_device), GFP_KERNEL);
    if(my_devices == NULL)
    {
        ret = -ENOMEM;
        printk(KERN_INFO "%s: kmalloc() is failed\n", THIS_MODULE->name);
        goto my_device_kmalloc_failed;
    }
    printk(KERN_INFO "%s : kmalloc is success\n", THIS_MODULE->name);

    for (i = 0; i < my_devcnt; i++)
    {
        ret = kfifo_alloc(&my_devices[i].my_buf, MAX, GFP_KERNEL);
        if (ret != 0)
        {
            printk(KERN_INFO "%s : kfifo_alloc() is failed for device %d\n", THIS_MODULE->name, i);
            goto kfifo_alloc_failed;
        }
    }
    printk(KERN_INFO "%s : kfifo_alloc is success\n", THIS_MODULE->name);

    // devname = my_char
    ret = alloc_chrdev_region(&devno, 0, my_devcnt, "my_char");
    if (ret != 0)
    {
        printk(KERN_INFO "%s : alloc_chrdev_region_failed\n", THIS_MODULE->name);
        goto alloc_chrdev_failed;
    }
    major = MAJOR(devno);
    minor = MINOR(devno);
    printk(KERN_INFO "%s : alloc_chrdev_region is success. major=%d, minor=%d, devno=%d\n", THIS_MODULE->name,major,minor,devno);
    // class = multidev
    pclass = class_create(THIS_MODULE,"multidev_char");
    if (IS_ERR(pclass))
    {
        printk(KERN_INFO "%s : class_create is failed\n", THIS_MODULE->name);
        ret = -1;
        goto class_create_failed;
    }
    printk(KERN_INFO "%s : class_create is success\n", THIS_MODULE->name);

    for (i = 0; i < my_devcnt; i++)
    {
        my_devices[i].my_devno = MKDEV(major, i);
        pdevices = device_create(pclass, NULL, my_devices[i].my_devno, NULL, "my_char%d", i);
        if (IS_ERR(pdevices))
        {
            printk(KERN_ERR "%s : device_create is failed for device %d\n", THIS_MODULE->name, i);
            ret = -1;
            goto device_create_failed;
        }
        printk(KERN_INFO "%s: %dth devices devno= %d\n", THIS_MODULE->name,i, my_devices[i].my_devno);
    }
    printk(KERN_INFO "%s : device_create is success\n", THIS_MODULE->name);

    for (i = 0; i < my_devcnt; i++)
    {
        cdev_init(&my_devices[i].my_cdev, &my_fops);
        ret = cdev_add(&my_devices[i].my_cdev,my_devices[i].my_devno,1);
        if (ret != 0)
        {
            printk(KERN_INFO "%s: cdev_add is failed\n", THIS_MODULE->name);
            goto cdev_add_failed;
        }
    }
    printk(KERN_INFO "%s : cdev_add is success\n", THIS_MODULE->name);

    return 0;

cdev_add_failed:
    for (i = i - 1; i >= 0; i--)
        cdev_del(&my_devices[i].my_cdev);
    i = my_devcnt;
device_create_failed:
    for (i = i - 1; i >= 0; i--)
    {
        device_destroy(pclass, my_devices[i].my_devno);
    }
    class_destroy(pclass);
class_create_failed:
    unregister_chrdev_region(devno, 1);
alloc_chrdev_failed:
    i = my_devcnt;
kfifo_alloc_failed:
    for (i = my_devcnt - 1; i >= 0; i++)
    {
        kfifo_free(&my_devices[i].my_buf);
    }
    kfree(my_devices);
my_device_kmalloc_failed:
    return ret;
}

static __exit void pchar_exit(void)
{
    int i;
    dev_t devno=MKDEV(major,0);
    printk(KERN_INFO "%s : pchar_exit is called\n", THIS_MODULE->name);
    for (i = my_devcnt - 1; i >= 0; i--)
        cdev_del(&my_devices[i].my_cdev);
    printk(KERN_INFO "%s : cdev_del remove devices from kernle db\n", THIS_MODULE->name);
    for (i = my_devcnt - 1; i >= 0; i--)
    {
        device_destroy(pclass, my_devices[i].my_devno);
    }
    printk(KERN_INFO "%s : device_destroy() destroy device files\n", THIS_MODULE->name);
    class_destroy(pclass);
    printk(KERN_INFO "%s : class_destroy() destroy device class\n", THIS_MODULE->name);
    unregister_chrdev_region(devno,my_devcnt);
    printk(KERN_INFO "%s : unregister_chrdev_region is success\n", THIS_MODULE->name);
    for (i = my_devcnt-1; i >= 0; i--)
    {
        kfifo_free(&my_devices[i].my_buf);
    }
    printk(KERN_INFO "%s : kfifo free all buf are release\n", THIS_MODULE->name);
    kfree(my_devices);
    printk(KERN_INFO "%s : kfree released devcices private struct memory\n", THIS_MODULE->name);
}

static int pchar_open(struct inode *pinode, struct file *pfile)
{   
    printk(KERN_INFO "%s : pchar_open is called\n",THIS_MODULE->name);
    struct pchar_device *pdev = 
    container_of(pinode->i_cdev,struct pchar_device,my_cdev);
    pfile->private_data = pdev;
    return 0;
}

static int pchar_close(struct inode *pinode, struct file *pfile)
{
    printk(KERN_INFO "%s:char_close is called\n",THIS_MODULE->name);
    return 0;
}

static ssize_t pchar_read(struct file *pfile, char *ubuf, size_t size, loff_t *poffset)
{
    int nbytes=0,ret;
    printk(KERN_INFO "%s : pchar_read is called\n", THIS_MODULE->name);
    struct pchar_device *pdev = (struct pchar_device*)pfile->private_data;
    ret = kfifo_to_user(&pdev->my_buf,ubuf,size, &nbytes);
    if(ret < 0){
        printk(KERN_ERR"%s: pchar_read is failed to copy data from user to kernel space\n",THIS_MODULE->name);
        return ret;
    }
    printk(KERN_INFO"%s : bytes read from user space %d\n",THIS_MODULE->name,nbytes);
    return nbytes;
}

static ssize_t pchar_write(struct file *pfile, const char *ubuf, size_t size, loff_t *poffset)
{
    int nbytes=size,ret;
    printk(KERN_INFO "%s : pchar_write is called\n", THIS_MODULE->name);
    struct pchar_device *pdev = (struct pchar_device*)pfile->private_data; 
    ret = kfifo_from_user(&pdev->my_buf,ubuf,size, &nbytes);
    if(ret < 0){
        printk(KERN_ERR"%s: pchar_write is failed to copy data from kernel to user space\n",THIS_MODULE->name);
        return ret;
    }
    printk(KERN_INFO"%s : bytes write to user space %d\n", THIS_MODULE->name,nbytes);
    return nbytes;
}

static long pchar_ioctl(struct file *pfile, unsigned int cmd, unsigned long param){
    info_t info;
    int ret;
    struct pchar_device *pdev = (struct pchar_device *)pfile->private_data;
    switch(cmd){
        case FIFO_CLEAR:
            printk(KERN_INFO"%s : pchar_ioctl() fifo clear\n", THIS_MODULE->name);
            kfifo_reset(&pdev->my_buf);
            break;

        case FIFO_INFO:
            printk(KERN_INFO"%s : pchar_ioctl() fifo info\n", THIS_MODULE->name);
            info.size = kfifo_size(&pdev->my_buf);
            info.avail = kfifo_avail(&pdev->my_buf);
            info.len = kfifo_len(&pdev->my_buf);
            ret = copy_to_user((void*)param,&info,sizeof(info_t));
            break;

        case FIFO_RESIZE:
            
            void *temp_buf;
            printk(KERN_INFO"%s : pchar_ioctl() fifo resize\n", THIS_MODULE->name);
            // allocate temp buffer of fifo length - kmalloc(), kifo_len()
            int len = kfifo_len(&pdev->my_buf);
            temp_buf = kmalloc(len ,GFP_KERNEL);
            if(temp_buf == NULL)
            {
                ret = -ENOMEM;
                printk(KERN_INFO "%s: kmalloc() is failed\n", THIS_MODULE->name);
            }
            printk(KERN_INFO"%s : pchar_ioctl() kmalloc done\n", THIS_MODULE->name);
            // copy fifo data into that buffer - kfifo_out()
            ret = kfifo_out(&pdev->my_buf,temp_buf,len);
            if (ret == 0){
                printk(KERN_ERR"%s: kfifo_out failed \n",THIS_MODULE->name);
                kfree(temp_buf);
            }
            printk(KERN_INFO"%s : pchar_ioctl() kfifo_out done\n", THIS_MODULE->name);
            // release kfifo memory - kfifo_free()
            kfifo_free(&pdev->my_buf);
            printk(KERN_INFO"%s : pchar_ioctl() kfifo free done\n", THIS_MODULE->name);
            // allocate kfifo with new size - kfifo_alloc() with param
            ret = kfifo_alloc(&pdev->my_buf,param,GFP_KERNEL);
            if (ret != 0)
            {
                printk(KERN_INFO "%s : kfifo_alloc() is failed\n", THIS_MODULE->name);
                kfree(temp_buf);
            }
            printk(KERN_INFO"%s : pchar_ioctl() kfifo_alloc done\n", THIS_MODULE->name);
            // copy temp buffer data back to fifo - kfifo_in()
            ret = kfifo_in(&pdev->my_buf,temp_buf,len);
            if (ret == 0){
                printk(KERN_ERR"%s: kfifo_out failed \n",THIS_MODULE->name);
                kfree(temp_buf);
            }
            printk(KERN_INFO"%s : pchar_ioctl() kfifo in done\n", THIS_MODULE->name);
            // release temp buffer - kfree()
            kfree(temp_buf);
            printk(KERN_INFO"%s : pchar_ioctl() kfree done\n", THIS_MODULE->name);
            info.size = kfifo_size(&pdev->my_buf);
            info.avail = kfifo_avail(&pdev->my_buf);
            info.len = kfifo_len(&pdev->my_buf);
            printk(KERN_INFO"%s : pchar_ioctl() info.size=%d, info.avail=%d, info.len=%d\n", THIS_MODULE->name,info.size,info.avail,info.len);
            ret = copy_to_user((void*)param,&info,sizeof(info_t));
            break;
        default:
            printk(KERN_INFO"%s : pchar_ioctl() unspported cmd\n", THIS_MODULE->name);
            return -EINVAL;
    }
    return 0;
}

module_init(pchar_init);
module_exit(pchar_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Parth");
MODULE_DESCRIPTION("This multi-devices instance device driver");
