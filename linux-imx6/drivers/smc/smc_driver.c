#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/delay.h>
#include <asm/uaccess.h>

#include <linux/arm-smccc.h>
#include <linux/optee_smc.h>
#include <linux/slab.h>
#include <linux/ktime.h>
#include <linux/time.h>

/* For dma_mmap_coherent() and vm_area_stuct  */
#include <linux/dma-mapping.h>
#include <linux/io.h>
#include <linux/mm_types.h>


/* Shared memory between User and Kernel space (CMA coherent) */
static dma_addr_t dma_handle_pa;
static void *dma_handle_va;
static long dma_allocated_length = 0;


/*
 *  MODE 1: Use Shared Memory (buffer) between User Space and Kernel Space
 *  MODE 2: vcrypto mode
 *
 */
#define COHERENT 1
#define VCRYPTO 2

#define MODE COHERENT


/*
 *  FUNC_IDs in Rushmore API Library
 *
 */
#define FUNC_ID_INIT        1
#define FUNC_ID_RELEASE     2
#define FUNC_ID_RENDER      3
#define FUNC_ID_REMOVE      4
#define FUNC_ID_INVOKE      5


#define TIME_ARG_PASS       false
#define TIME_SMC_CALL       false


struct tee_img {
	int crypto_mode;
	uint8_t iv[16];
	uint8_t iv_len;
    int width, height;
    int x_pos, y_pos;
    int offset;
	size_t len;
} __attribute__((packed));


struct tee_arg {
    struct tee_img (*imgs)[];
    int num_tee_imgs;
    int func_id;
    bool single_img_render;

    void *buffer;
    size_t buffer_size;
} __attribute__((packed));


static ssize_t device_write(struct file *filp, void *addr, size_t len, loff_t *off)
{
	struct arm_smccc_res result;

	int n, m;
    void *vaddr;
    switch (MODE) {

        /* MODE 1: Shared Memory Between User Space and Kernel Space */
        case COHERENT:

            if (addr == NULL)  
                arm_smccc_smc(OPTEE_SMC_RUSHMORE_THREAD, NULL, NULL, NULL, 0, 0, 0, 0, &result);

            else {
                void *buffer_vaddr = NULL;
                void *img_vaddr = NULL;
                void *arg_vaddr = NULL;

                struct tee_arg *arg = (struct tee_arg*) addr;
                struct timespec sTime, eTime;

                if (TIME_ARG_PASS && (*arg).func_id == FUNC_ID_RENDER) 
                    getnstimeofday(&sTime);
            
                if ((*arg).func_id == FUNC_ID_INVOKE) {
                    
                    /* Copy user defined buffer (for general invoke function) */
                    buffer_vaddr = (void*) kzalloc((*arg).buffer_size, GFP_KERNEL);
                    memcpy(buffer_vaddr, (*arg).buffer, (*arg).buffer_size);
                    (*arg).buffer = virt_to_phys(buffer_vaddr);
                }

                else if ((*arg).func_id != FUNC_ID_INIT 
                        && (*arg).func_id != FUNC_ID_RELEASE
                        && (*arg).func_id != FUNC_ID_REMOVE) {

                    /* Copy tee_images in tee_arg */
                    img_vaddr = (void*) kzalloc(sizeof(struct tee_img) * (*arg).num_tee_imgs, GFP_KERNEL);

                    if ((*arg).imgs == NULL){
                        printk(" [!] Skip because (*arg).imgs == NULL\n");
                        return len;
                    }

                    memcpy(img_vaddr, (*arg).imgs, sizeof(struct tee_img) * (*arg).num_tee_imgs);
                    (*arg).imgs = virt_to_phys(img_vaddr);
                }

                /* Copy tee_arg to kernel space */
                arg_vaddr = (void*) kzalloc(sizeof(struct tee_arg), GFP_KERNEL);
                memcpy(arg_vaddr, arg, sizeof(struct tee_arg));

      
                if (TIME_ARG_PASS && (*arg).func_id == FUNC_ID_RENDER) {                
                    getnstimeofday(&eTime);
                    printk("[ArgPassing] - before, %ld, %ld\n", sTime.tv_sec, sTime.tv_nsec);
                    printk("[ArgPassing] - after, %ld, %ld\n", eTime.tv_sec, eTime.tv_nsec);
                }

                if (TIME_SMC_CALL && (*arg).func_id == FUNC_ID_RENDER)
                    getnstimeofday(&sTime);
                
                /* SMC call to Rushmore kernel in the SW */
                arm_smccc_smc(OPTEE_SMC_IOCLOAK_SET, 
                        dma_handle_pa, virt_to_phys(arg_vaddr), len, 0, 0, 0, 0, &result);

                if (TIME_SMC_CALL && (*arg).func_id == FUNC_ID_RENDER) {
                    getnstimeofday(&eTime);
                    printk("[ContextSwitch] - before, %ld, %ld\n", sTime.tv_sec, sTime.tv_nsec);
                    printk("[ContextSwitch] - after, %ld, %ld\n", eTime.tv_sec, eTime.tv_nsec);
                }


                if (buffer_vaddr != NULL) kfree(buffer_vaddr);
                if (img_vaddr != NULL) kfree(img_vaddr);
                if (arg_vaddr != NULL) kfree(arg_vaddr);
            } 
            return len;


	    /* MODE 2: Vcrypto. All it need is simply forwarding 8 bytes to SW */
	    case VCRYPTO:
	    {
		    struct arm_smccc_res result;
		    unsigned int *x = (unsigned int *)addr;
		    
            if (len < sizeof(unsigned int) * 2)
			    return -1;

            /* SMC call to Rushmore kernel in the SW */
		    arm_smccc_smc(OPTEE_SMC_IOCLOAK_SET, x[0], x[1], 0, 0, 0, 0, 0, &result);
		    return sizeof(unsigned int) * 2;
	    }
    }

    return len;
}


static int device_mmap(struct file *filp, struct vm_area_struct *vma) {
    int ret = 0;
    long length = vma->vm_end - vma->vm_start;

   
    dma_handle_va = dma_alloc_coherent (NULL, length, &dma_handle_pa, GFP_KERNEL);

    if (!dma_handle_va) { 
        printk(KERN_ERR " - dma_alloc_coherent error\n"); 
        return -ENOMEM; 
    }

    ret = dma_mmap_coherent(NULL, vma, dma_handle_va, dma_handle_pa, length);  
    dma_allocated_length = length;
    
    uint8_t *a = kzalloc(sizeof(uint8_t) * 1096 * 1096 * 5, GFP_KERNEL);
    memcpy(dma_handle_va, a, sizeof(uint8_t) * 1096 * 1096 * 5);
    printk("Allocated buffer size: %ld, try memcpying: %d\n", length, sizeof(uint8_t) * 1096 * 1096 * 5);
    kfree(a);

    return ret;
}

static int device_open(struct inode *inode, struct file *file)
{
  return 0;
}

static int device_release(struct inode *inode, struct file *file)
{
  if (MODE == COHERENT && dma_allocated_length != 0) {
      dma_free_coherent (NULL, 
              dma_allocated_length , dma_handle_va, dma_handle_pa);
  }
  return 0;
}

static struct file_operations fops = {
        .owner = THIS_MODULE,
        .write = device_write,
        .mmap = device_mmap,
        .open = device_open,
        .release = device_release,
};

static struct miscdevice smc_miscdev = {
        .name         = "smc_driver",
        .mode         = S_IRWXUGO,
        .fops         = &fops,
};

static int __init smc_driver_module_init(void)
{
  int ret = misc_register(&smc_miscdev);
  if (ret < 0) {
    printk ("Registering the smc driver failed\n");
    return ret;
  }
  printk("Create node with mknod /dev/smc_driver\n");
  return 0;
}

static void __exit smc_driver_module_exit(void)
{
  misc_deregister(&smc_miscdev);
}  

module_init(smc_driver_module_init);
module_exit(smc_driver_module_exit);
MODULE_LICENSE("GPL");
