#include "sysinfo.h"

#include <QtGlobal>
#if defined (Q_OS_LINUX)
 #include <sys/sysinfo.h>
 #include <stdlib.h>
#elif defined (Q_OS_OSX)
 #include <sys/types.h>
 #include <sys/sysctl.h>
 #include <stdlib.h>
 #import <mach/host_info.h>
 #import <mach/mach_host.h>
 #import <mach/task_info.h>
 #import <mach/task.h>
#elif defined (Q_OS_WIN)
 #include <windows.h>
#else
 #include <QThread>
#endif

float loadavg1()
{
#if defined (Q_OS_UNIX)
  double loadavg[1] = {0};
  getloadavg(loadavg, 1);
  return loadavg[0];
#else
  return 0;
#endif
}

int numCpu()
{
#ifdef Q_OS_LINUX
  return get_nprocs_conf();
#elif defined(Q_OS_OSX)
  int ncpu, mib[2] = { CTL_HW, HW_NCPU };
  size_t len = sizeof(ncpu);
  sysctl(mib, 2, &ncpu, &len, NULL, 0);
  return ncpu;
#elif defined (Q_OS_WIN)
  SYSTEM_INFO sysinfo;
  GetSystemInfo( &sysinfo );
  return(sysinfo.dwNumberOfProcessors);
#else
  return QThread::idealThreadCount();
#endif
}

struct memory_info memInfo()
{
  struct memory_info result = { 0, 0 };

#ifdef Q_OS_LINUX
  struct sysinfo sinfo;
  if (0 == sysinfo(&sinfo)) {
    result.totalKB = sinfo.totalram / 1024;
    result.freeKB = sinfo.freeram / 1024;
  }
#elif defined(Q_OS_OSX)

  int mibtotal[2] = { CTL_HW, HW_MEMSIZE };
  int64 physical_memory;
  size_t len = sizeof(physical_memory);
  sysctl(mibtotal, 2, &physical_memory, &len, NULL, 0);
  result.totalKB = physical_memory / 1024;

  int mibogsize[1] = { CTL_HW, HW_PAGESIZE };
  int pagesize;
  len = sizeof(pagesize);
  sysctl (mibogsize, 2, &pagesize, &length, NULL, 0);

  vm_statistics_data_t vmstat;
  if (host_statistics (mach_host_self (), HOST_VM_INFO, (host_info_t) &vmstat, &count) == KERN_SUCCESS)
    result.freeKB = free(vmstat.free_count * pagesize / 1024)

#elif defined (Q_OS_WIN)
  MEMORYSTATUSEX memory_status;
  ZeroMemory(&memory_status, sizeof(MEMORYSTATUSEX));
  memory_status.dwLength = sizeof(MEMORYSTATUSEX);
  if (GlobalMemoryStatusEx(&memory_status)) {
    result.totalKB = memory_status.ullTotalPhys / 1024;
    result.freeKB = memory_status.ullAvailPhys / 1024;
  }
#endif

  return result;
}
