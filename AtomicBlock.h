static inline bool __BLOCK_ENTER(void) {
  __asm__ volatile ("" ::: "memory");
  noInterrupts();
  return true;
}

static inline void __BLOCK_EXIT(bool*) {
  interrupts();
  __asm__ volatile ("" ::: "memory");
}

#define ATOMIC_BLOCK for (bool __scope __attribute__((__cleanup__(__BLOCK_EXIT))) = __BLOCK_ENTER(); __scope ; __scope = false )
