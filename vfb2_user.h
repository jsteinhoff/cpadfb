#ifndef _LINUX_VFB2_USER_H
#define _LINUX_VFB2_USER_H

#include <linux/ioctl.h>
#include "vfb2.h"

#define UVFB2_DEF_NUM_MODES	16

#define UVFB2_DEVICE		"driver/userfb"

#define UVFB2_IOCTL_BASE	0x00

/* if you want to support more than 16 video modes, call this first */
#define UVFB2_NUM_MODES		_IOW('F', UVFB2_IOCTL_BASE, int)

/* add one mode */
#define UVFB2_ADD_MODE		_IOW('F', UVFB2_IOCTL_BASE+1, struct vfb2_mode)

/* set size of the videomemory and register the frame buffer
 * after this ioctl was called, you can not add modes any more
 */
#define UVFB2_VMEM_SIZE		_IOW('F', UVFB2_IOCTL_BASE+2, __u32)

/* returns wich mode is currently active, starting with 0 */
#define UVFB2_MODE		_IOR('F', UVFB2_IOCTL_BASE+3, int)

/* returns the node number of the fb device */
#define UVFB2_NODE		_IOR('F', UVFB2_IOCTL_BASE+4, int)

/* to unregister the frame buffer, just close the file */

#endif /* _LINUX_VFB2_USER_H */
