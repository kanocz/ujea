#include <QtGlobal>
#if defined (Q_OS_LINUX)
 #include <sys/sysinfo.h>
 #include <stdlib.h>
#elif defined (Q_OS_OSX)
 #include <sys/types.h>
 #include <sys/sysctl.h>
 #include <stdlib.h>
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
