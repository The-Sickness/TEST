#ifndef _PARISC_SIGINFO_H
#define _PARISC_SIGINFO_H

<<<<<<< HEAD
#if defined(__LP64__)
#define __ARCH_SI_PREAMBLE_SIZE   (4 * sizeof(int))
#endif

=======
>>>>>>> 512ca3c... stock
#include <asm-generic/siginfo.h>

#undef NSIGTRAP
#define NSIGTRAP	4

#endif
