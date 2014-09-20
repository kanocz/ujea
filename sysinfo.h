#ifndef SYSINFO_H
#define SYSINFO_H

struct memory_info {
  int totalKB;
  int freeKB;
};

float loadavg1();
int numCpu();
struct memory_info memInfo();

#endif // SYSINFO_H
