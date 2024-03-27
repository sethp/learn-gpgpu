#ifndef __gdb_h__
#define __gdb_h__

#if NDEBUG
#define GDB_BREAK
#else
#define GDB_BREAK                                                              \
  do                                                                           \
  {                                                                            \
    __asm__("int $3\n");                                                       \
    __asm__("nop");                                                            \
  } while (0);
#endif

#endif
