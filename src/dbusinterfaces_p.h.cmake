#ifndef QAPT_DBUSINTERFACES_P_H
#define QAPT_DBUSINTERFACES_P_H

#include "transactioninterface_p.h"
#include "workerinterface_p.h"

// Unlike other approaches this one will always explode when a define does not
// actually make it into the compiler (e.g. because the CMake var was renamed).
// e.g. QString() would compile fine if the define is missing,
// char[]=; on the other hand won't.
static const char s_workerVersion[] = @QAPT_WORKER_VERSION_STRING@;
static const char s_workerReverseDomainName[] = @QAPT_WORKER_RDN_VERSIONED_STRING@;

// This will fail regardless of whether the var is present or not as this is
// hard typed anyway.
typedef OrgKubuntuQaptworker@QAPT_WORKER_VERSION@Interface WorkerInterface;
typedef OrgKubuntuQaptworker@QAPT_WORKER_VERSION@TransactionInterface TransactionInterface;

#endif // QAPT_DBUSINTERFACES_P_H
