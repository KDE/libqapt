#ifndef QAPT_URIHELPER_P_H
#define QAPT_URIHELPER_P_H

#include <QString>

// Unlike other approaches this one will always explode when a define does not
// actually make it into the compiler (e.g. because the CMake var was renamed).
// e.g. QString() would compile fine if the define is missing,
// char[]=; on the other hand won't.
static const char s_workerVersion[] = @QAPT_WORKER_VERSION_STRING@;
static const char s_workerReverseDomainName[] = @QAPT_WORKER_RDN_VERSIONED_STRING@;

static QString dbusActionUri(const char *actionId)
{
    return QString("%1.%2").arg(QLatin1String(s_workerReverseDomainName),
                                QLatin1String(actionId));
}

#endif // QAPT_URIHELPER_P_H
