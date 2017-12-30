#ifndef _LINUX_VFB2_H
#define _LINUX_VFB2_H

#include <asm/types.h>
#include <linux/fb.h>


#define VFB2_16BPP_NO_TRANSP	0
#define VFB2_16BPP_TRANSP	1

struct vfb2_mode {
	__u32 xres;
	__u32 yres;
	__u32 bpp;
	__u32 visual;
	__u8 transp_mode;
	__u8 reserved[3]; 
};


#ifdef __KERNEL__

struct vfb2_init {
	__u32 vmem_len;
	struct vfb2_mode *mode_table;
	int (*vfb2_ioctl)(struct inode *inode, struct file *file,
			  unsigned int cmd, unsigned long arg, int table_index);
	void *private;
};

int vfb2_register(struct vfb2_init *init);
void vfb2_unregister(int table_index);
int vfb2_current_mode(int table_index);
void *vfb2_videomemory(int table_index);
struct fb_info *vfb2_fb_info(int table_index);
void *vfb2_private(int table_index);

#endif /* __KERNEL__ */

#endif /* _LINUX_VFB2_H */
