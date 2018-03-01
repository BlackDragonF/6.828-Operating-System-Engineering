// User-level IPC library routines

#include <inc/lib.h>

// Receive a value via IPC and return it.
// If 'pg' is nonnull, then any page sent by the sender will be mapped at
//	that address.
// If 'from_env_store' is nonnull, then store the IPC sender's envid in
//	*from_env_store.
// If 'perm_store' is nonnull, then store the IPC sender's page permission
//	in *perm_store (this is nonzero if a page was successfully
//	transferred to 'pg').
// If the system call fails, then store 0 in *fromenv and *perm (if
//	they're nonnull) and return the error.
// Otherwise, return the value sent by the sender
//
// Hint:
//   Use 'thisenv' to discover the value and who sent it.
//   If 'pg' is null, pass sys_ipc_recv a value that it will understand
//   as meaning "no page".  (Zero is not the right value, since that's
//   a perfectly valid place to map a page.)
int32_t
ipc_recv(envid_t *from_env_store, void *pg, int *perm_store)
{
	// LAB 4: Your code here.
    // check pg, if pg is null, then should set it to UTOP,
    // for only if dstva is below UTOP, sender knows that
    // receiver wants a page to be transferred
    if (!pg) {
        pg = (void *)UTOP;
    }
    int r;
    if ((r = sys_ipc_recv(pg)) < 0) {
        // system call fails
        // store from env and perm if corresponding pointer is not null
        if (from_env_store) {
            *from_env_store = 0;
        }
        if (perm_store) {
            *perm_store = 0;
        }
        // return the error
        return r;
    }

    // system call succeeds
    // extract information from thisenv's IPC fields
    if (from_env_store) {
        // store IPC sender's envid into *from_env_store
        *from_env_store = thisenv->env_ipc_from;
    }
    if (perm_store) {
        // store IPC sender's page permission
        *perm_store = thisenv->env_ipc_perm;
    }

	// panic("ipc_recv not implemented");

    // return the value send by the sender
    return thisenv->env_ipc_value;
}

// Send 'val' (and 'pg' with 'perm', if 'pg' is nonnull) to 'toenv'.
// This function keeps trying until it succeeds.
// It should panic() on any error other than -E_IPC_NOT_RECV.
//
// Hint:
//   Use sys_yield() to be CPU-friendly.
//   If 'pg' is null, pass sys_ipc_try_send a value that it will understand
//   as meaning "no page".  (Zero is not the right value.)
void
ipc_send(envid_t to_env, uint32_t val, void *pg, int perm)
{
	// LAB 4: Your code here.
    // check pg, if pg is null, then should set it to UTOP(invalid value
    // but won't result in panic)
    if (!pg) {
        pg = (void *)UTOP;
    }
    int r;
    while(true) {
        // keeps trying until succeeds or panic
        r = sys_ipc_try_send(to_env, val, pg, perm);
        if (r == 0) {
            // message successfully sent, return
            return;
        }
        if (r != -E_IPC_NOT_RECV) {
            // error when sending, should panic
            panic("failed to send messages - %e!", r);
        }
        // use sys_yield to avoid waste on CPU
        sys_yield();
    }

	// panic("ipc_send not implemented");
}

// Find the first environment of the given type.  We'll use this to
// find special environments.
// Returns 0 if no such environment exists.
envid_t
ipc_find_env(enum EnvType type)
{
	int i;
	for (i = 0; i < NENV; i++)
		if (envs[i].env_type == type)
			return envs[i].env_id;
	return 0;
}
