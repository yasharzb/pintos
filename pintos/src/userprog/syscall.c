#include "userprog/syscall.h"
#include <stdio.h>
#include <string.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "threads/vaddr.h"
#include "pagedir.h"
#include "threads/palloc.h"

static void syscall_handler(struct intr_frame *);
uint32_t *assign_args(uint32_t *esp);
void *copy_user_mem_to_kernel(void *src, int size, bool null_terminated);
void *get_kernel_va_for_user_pointer(void *ptr);

void syscall_init(void)
{
  intr_register_int(0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler(struct intr_frame *f UNUSED)
{
  bool success = true;
  uint32_t *args = assign_args((uint32_t *)f->esp);

  /*
   * The following print statement, if uncommented, will print out the syscall
   * number whenever a process enters a system call. You might find it useful
   * when debugging. It will cause tests to fail, however, so you should not
   * include it in your final submission.
   */

  /* printf("System call number: %d\n", args[0]); */

  if (args[0] == SYS_EXIT)
  {
    f->eax = args[1];
    printf("%s: exit(%d)\n", &thread_current()->name, args[1]);
    thread_exit();
  }

  if (args[0] == SYS_WRITE) // int write (int fd, const void *buffer, unsigned size)
  {
    // args[2] is a pointer so we need to validate it:
    void *buffer = get_kernel_va_for_user_pointer((void *)args[2]);
    if (buffer == NULL)
    {
      success = false;
      goto done;
    }

    if (args[1] == STDOUT_FILENO)
    {
      putbuf((char *)buffer, args[3]);
    }
    palloc_free_page(buffer);
  }

  if (args[0] == SYS_PRACTICE) // int practice (int i)
  {
    f->eax = args[1] + 1;
  }

done:
  if (!success)
    f->eax = -1;
  palloc_free_page(args);
}

uint32_t *
assign_args(uint32_t *esp)
{
  /* check if stack pointer is not corrupted */
  if (!is_user_vaddr(esp))
    return false;

  /* copy arguments in user stack to kernel */
  char *buffer = copy_user_mem_to_kernel(esp, MAX_SYSCALL_ARGS * 4, false);
  if (buffer == NULL)
    return false;

  return (uint32_t *)buffer;
}

/* copy user memory pointed by `ptr` to kernel memory
   and return pointer to allocated address */
void *
get_kernel_va_for_user_pointer(void *ptr)
{
  if (ptr == NULL || !is_user_vaddr(ptr))
    return NULL;

  char *buffer = copy_user_mem_to_kernel(ptr, MAX_SYSCALK_ARG_LENGTH, true);
  if (buffer == NULL)
    return NULL;

  /* set last byte in page to zero so kernel don't die in case of not valid string */
  buffer[MAX_SYSCALK_ARG_LENGTH] = 0;
  return (void *)buffer;
}

/* copy from src in user virtual address to dst in kernel virtual
    address. if null_terminated it will continue until reaching zero,
    else it will copy `size` bytes. Max size can be PAGE_SIZE.
    (you can implement o.w. if you need.)
    will return NULL if fail. */
void *
copy_user_mem_to_kernel(void *src, int size, bool null_terminated)
{
  if (size > PGSIZE)
    return NULL;

  /* allocate a page to store data */
  char *buffer = palloc_get_page(0);
  if (buffer == NULL)
    goto fail;

  char *current_address = src;
  int copied_size = 0;
  while (copied_size < size)
  {
    if (!is_user_vaddr(current_address))
      goto fail;

    /* maximum size that we can copy in this page */
    int cur_size = (uintptr_t)pg_round_up((void *)current_address) - (uintptr_t)current_address + 1;
    if (size - copied_size < cur_size)
      cur_size = size - copied_size;

    /* get kernel virtual address corresponding to currrent address */
    char *kernel_address = pagedir_get_page(thread_current()->pagedir, (void *)current_address);
    if (kernel_address == NULL)
      goto fail;

    memcpy(buffer + copied_size, kernel_address, cur_size);

    /* we will search for null here if we need to copy until null */
    if (null_terminated)
    {
      char *temp = buffer + copied_size;
      bool null_found = false;
      while (temp < buffer + copied_size + cur_size)
      {
        if (!(*temp))
          null_found = true;
        temp++;
      }
      // if null found stop copy.
      if (null_found)
        break;
    }

    copied_size += cur_size;
    current_address += cur_size;
  }

  return buffer;

fail:
  if (buffer)
    palloc_free_page(buffer);
  return NULL;
}
