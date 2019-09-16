#ifndef __ERRNO_H__
#define __ERRNO_H__

#define errno (*__errno())
[[cheerp::genericjs]]
extern "C" int *__errno _PARAMS ((void));

#define	EBADF 9		/* Bad file number */
#define	EAGAIN 11	/* No more processes */
#define ENETUNREACH 114		/* Network is unreachable */
#define ENETDOWN 115		/* Network interface is not configured */
#define EMSGSIZE 122		/* Message too long */
#define EADDRNOTAVAIL 125	/* Address not available */
#define EWOULDBLOCK EAGAIN	/* Operation would block */

#endif
