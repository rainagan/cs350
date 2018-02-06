#include <types.h>
#include <kern/errno.h>
#include <kern/unistd.h>
#include <kern/wait.h>
#include <lib.h>
#include <syscall.h>
#include <current.h>
#include <proc.h>
#include <thread.h> 
#include <addrspace.h>
#include <copyinout.h>
#include "opt-A2.h"
#include <synch.h>
#include <mips/trapframe.h>
#include <limits.h>
#include <vm.h>
#include <vfs.h>
#include <test.h>
#include <kern/fcntl.h> 


  /* this implementation of sys__exit does not do anything with the exit code */
  /* this needs to be fixed to get exit() and waitpid() working properly */

void sys__exit(int exitcode) {
  #if OPT_A2
  struct addrspace *as;
  struct proc *p = curproc;
  /* for now, just include this to keep the compiler from complaining about
     an unused variable */

  DEBUG(DB_SYSCALL,"Syscall: _exit(%d)\n",exitcode);
  KASSERT(curproc->p_addrspace != NULL);

  /* remove p's children */
  lock_acquire(arr_proc_lock);
  unsigned length = array_num(&arr_proc);
  // kprintf("length = %d\n",length);
  for (unsigned i=0; i<length; ++i) {
    struct proc *temp = array_get(&arr_proc,i);
    if (temp->parent == curproc->PID) {
      temp->parent = -1;
      lock_acquire(temp->exit_lock);
      cv_broadcast(temp->exit_cv, temp->exit_lock);
      lock_release(temp->exit_lock);
    }
  }
  lock_release(arr_proc_lock); 

  /* set p's exit code */
  p->exit_code = _MKWAIT_EXIT(exitcode);
//  kprintf("p->exit_code = %d\n", p->exit_code);  
  /* set p's exit status t'';,o true */
  p->exit_status = true;

  /* wake up the parent wating for p's exit code */
  lock_acquire(p->wait_lock);
  cv_broadcast(p->wait_cv, p->wait_lock);
  lock_release(p->wait_lock);

  lock_acquire(p->exit_lock);
  while (p->parent != -1) {
    cv_wait(p->exit_cv, p->exit_lock);
  }
  lock_release(p->exit_lock);

  as_deactivate();
  /*
   * clear p_addrspace before calling as_destroy. Otherwise if
   * as_destroy sleeps (which is quite possible) when we
   * come back we'll be calling as_activate on a
   * half-destroyed address space. This tends to be
   * messily fatal.
   */
  as = curproc_setas(NULL);
  as_destroy(as);

  /* detach this thread from its process */
  /* note: curproc cannot be used after this call */
  proc_remthread(curthread);

  /* if this is the last user process in the system, proc_destroy()
     will wake up the kernel menu thread */
  proc_destroy(p);
  
  thread_exit();
  /* thread_exit() does not return, so we should never get here */
  panic("return from thread_exit in sys_exit\n");
  #endif
}


/* stub handler for getpid() system call                */
int
sys_getpid(pid_t *retval)
{
  /* for now, this is just a stub that always returns a PID of 1 */
  /* you need to fix this to make it work properly */
#if OPT_A2
  KASSERT(curproc != NULL);
  *retval = curproc->PID;
#else
  *retval = 1;
#endif
  return(0);
}

/* stub handler for waitpid() system call                */

int
sys_waitpid(pid_t pid,
  userptr_t status,
  int options,
  pid_t *retval)
{
  int exitstatus;
  int result;

  /* this is just a stub implementation that always reports an
     exit status of 0, regardless of the actual exit status of
     the specified process.   
     In fact, this will return 0 even if the specified process
     is still running, and even if it never existed in the first place.
     Fix this!
  */

  if (options != 0) {
    return(EINVAL);
  }

  #if OPT_A2
  /* check if curproc has a child with given pid */
  struct proc *c = NULL;
  lock_acquire(arr_proc_lock);
  int length = array_num(&arr_proc);
//  kprintf("curproc->children.size = %d",length);
  for (int i=0; i<length; ++i) {
    struct proc *temp = array_get(&arr_proc,i);
    if (pid == temp->PID) {
      c = temp;
      break;
    }
  }
  lock_release(arr_proc_lock);
  if (c == NULL) {
    return ECHILD;
  }
// kprintf("input pid = %d", pid);

  /* if c is not exited, curproc wait until it exits */
  lock_acquire(c->wait_lock);
  while(!c->exit_status) {
    cv_wait(c->wait_cv, c->wait_lock);
  }
  /* once child exits, set exitstatus to child's exit code */ 
  lock_release(c->wait_lock); 
  exitstatus = c->exit_code;

//kprintf("exitstatus=%d\n",exitstatus);
#else
  /* for now, just pretend the exitstatus is 0 */
  exitstatus = 0;
  
#endif
  result = copyout((void *)&exitstatus,status,sizeof(int));
  if (result) {
    return(result);
  }
  *retval = pid;
  return(0);
}

#if OPT_A2
int sys_fork(struct trapframe *tf, pid_t *retval) {
    // kprintf("called sys_fork\n");
    // struct lock *lock;
    /* create proc structure for child process */
  struct proc *c = proc_create_runprogram("c");
    /* check error for creating a child */
    // kprintf("create child%d\n", c->PID);
  if (c == NULL) {
    // panic("cannot create child\n");
    return ENOMEM;   // out of memory
  }
  // assign_pid(c);
    /* create and copy addr space */
  int as = as_copy(curproc->p_addrspace,&(c->p_addrspace));
    // kprintf("copy parent's addr space\n");
    /* 
     check if as_copy returns error. If error, return immediately
     as_copy returns 0 if successed. otherwise, it returns a positive integer
     */
  if (as) {
    proc_destroy(c);
//  panic("cannot copy parent's addr space\n");
    return ENOMEM;
  }
    /* provide mutual exclusion */
    // lock_acquire(lock);
    /* add parent-child relationship */
  int temp;
  lock_acquire(arr_proc_lock);
  temp = array_add(&arr_proc, c, NULL);
  c->parent = curproc->PID;
  lock_release(arr_proc_lock);

    /* check if add_child failed */
  if (temp) {
    proc_destroy(c);
//  panic("cannot add child to curproc\n");
    return ENOMEM;
  }
//    c->parent = curproc;
    // kprintf("add parnet-child relationship\n");

    /* create a copy of trapframe */
  struct trapframe *new_tf = kmalloc(sizeof(struct trapframe));
  if (new_tf == NULL) {
    proc_destroy(c);
        //  panic("cannot allocate a trapframe\n");
    return ENOMEM;
  }

    /* copy everything from tf to new_tf without synchronization */
  memcpy(new_tf,tf,sizeof(struct trapframe));
    // kprintf("children copies curproc's trapframe\n");
  *retval = c->PID;
  int rv = thread_fork(c->p_name, c, &enter_forked_process, new_tf, 0);

    /* if unable to add a thread to children */
  if (rv) {
    proc_destroy(c);
    kfree(new_tf);
        //  panic("cannot add thread to child\n");
    return ENOMEM;
  }
  return 0;
}

int sys_execv(userptr_t program, userptr_t args) {
  KASSERT(program!=NULL);
  /* copy from runprogram */
  /* get the current addr space */
  struct addrspace *as = curproc_getas();
  KASSERT(as!=NULL);
  struct addrspace *new_as;
  struct vnode *v;
  vaddr_t entrypoint, stackptr;
  int result;
  /* count_arg indicates how many arguments we have */
  int count_arg = 0;

  /* cast parameter */
  char **temp_args = (char **) args;
//  const char *temp_program = (const char *) program;

  /* count how many arguments we have and pass them to the kernel */  
  while (temp_args[count_arg] != NULL) {
    ++count_arg;
  }
  if (count_arg > ARG_MAX) return E2BIG;

  // kprintf("num of args = %d", count_arg);

  /* create array for *args to store ptrs to strings */
  /* add 1 to count_args because of the terminal string */
  char **new_args = kmalloc(sizeof(char *) * (count_arg+1));
  if (new_args==NULL) return ENOMEM;
  for (int i=0; i<count_arg; ++i) {
    int length = strlen(temp_args[i])+1;
    new_args[i] = kmalloc(length * sizeof(char));
    result = copyinstr((userptr_t)temp_args[i], new_args[i], length, NULL);
    if (result) {
      kfree(new_args);
      return EFAULT;
    }
  }
  /* add a terminal string */
  new_args[count_arg] = NULL; 

  /* copy program path from user addr to kernel */
  /* length of program path = number of characters + 1 */
  size_t path_length = strlen((char *)program)+1;
  /* create array for program path */
  char *new_program = kmalloc(sizeof(char) * path_length);
  if (new_program==NULL) return ENOMEM;
  result = copyinstr(program, new_program, path_length, NULL);
  if (result) return EFAULT;

  /* Open the file. */
  // char *program_temp;
  //program_temp = kstrdup((char *)program);
  result = vfs_open(new_program, O_RDONLY, 0, &v);
  if (result) return ENODEV;
  //kfree(program_temp);

  
 /* delete old addr space */
  as_deactivate();
  as_destroy(as);

 /* Create a address space. */
  new_as = as_create();
  if (new_as ==NULL) {
    vfs_close(v);
    return ENOMEM;
  }
  /* Switch to it and activate it. */
  curproc_setas(new_as);
  as_activate();

  /* Load the executable. */
  result = load_elf(v, &entrypoint);
  if (result) {
    /* p_addrspace will go away when curproc is destroyed */
    vfs_close(v);
    return ENODEV;
  }

  /* Done with the file now. */
  vfs_close(v);

  /* Define the user stack in the address space */
  result = as_define_stack(new_as, &stackptr);
  if (result) {
    curproc_setas(as);
    return ENOMEM;
  } 

  /* copy argv to user space */  
  vaddr_t arg_ptr[count_arg+1];

  for (int i=count_arg-1;i>=0;--i) {
    /* modify stack pointer */
    int length = strlen(temp_args[i])+1;
    stackptr -= length;
    result = copyoutstr(new_args[i],(userptr_t)stackptr,length,NULL);
    if (result) {
      kfree(new_args);
      kfree(new_program);
      return ENOMEM;
    }
    /* save the argv's addr into vaddr_t array */
    arg_ptr[i] = stackptr;
  }
  /* set addr of terminator to null */
  arg_ptr[count_arg] = (vaddr_t)NULL;

/* string pointers must start at an address that is divisible by 8 */
  while (stackptr%8 != 0) {
    --stackptr;
  }
  // kprintf("stackptr = %d\n", stackptr);

  for (int i=count_arg; i>=0; --i) {
    /* ptrs must start at an address that is divisible by 4 */
    stackptr -= ROUNDUP(sizeof(vaddr_t), 4);
    // kprintf("stackptr = %d\n", stackptr);
    result = copyout(&arg_ptr[i], (userptr_t)stackptr, sizeof(vaddr_t));
//    kprintf("result of copyout argv addr to user space = %d\n", result);
    if (result) return ENOMEM;
  }


  /* Warp to user mode. */
  enter_new_process(count_arg, (userptr_t)stackptr, stackptr, entrypoint);
  
  /* enter_new_process does not return. */
  panic("enter_new_process returned\n");
  return EINVAL;
}
#endif 

