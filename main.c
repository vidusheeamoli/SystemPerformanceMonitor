/**
 * Tony Givargis
 * Copyright (C), 2023
 * University of California, Irvine
 *
 * CS 238P - Operating Systems
 * main.c
 */

#include "system.h"
#include <signal.h>
#include <stdlib.h>
#include <sys/sysinfo.h>
#include <unistd.h>

/**
 * Needs:
 *   signal()
 */

static volatile int done;

static void _signal_(int signum) {
  assert(SIGINT == signum);

  done = 1;
}

double cpu_util(const char *s) {
  static unsigned sum_, vector_[7];
  unsigned sum, vector[7];
  const char *p;
  double util;
  uint64_t i;

  if (!(p = strstr(s, " ")) ||
      (7 != sscanf(p, "%u %u %u %u %u %u %u", &vector[0], &vector[1],
                   &vector[2], &vector[3], &vector[4], &vector[5],
                   &vector[6]))) {
    return 0;
  }
  sum = 0;
  for (i = 0; i < ARRAY_SIZE(vector); ++i) {
    sum += vector[i];
  }
  util = (1.0 - (vector[3] - vector_[3]) / (double)(sum - sum_)) * 100.0;
  sum_ = sum;
  for (i = 0; i < ARRAY_SIZE(vector); ++i) {
    vector_[i] = vector[i];
  }
  return util;
}

double get_cpu_util() {
  const char *const PROC_STAT = "/proc/stat";
  char line[1024];
  FILE *file;
  if (!(file = fopen(PROC_STAT, "r"))) {
    TRACE("fopen()");
    return -1;
  }
  if (fgets(line, sizeof(line), file)) {
    fclose(file);
    return cpu_util(line);
  }
  return 0.0;
}

double swap_activity() {
  const char *const PROC_VMSTAT = "/proc/vmstat";
  FILE *file;
  char line[1024];
  unsigned long pagesSwappedIn = 0, pagesSwappedOut = 0;
  double swapActivity;

  file = fopen(PROC_VMSTAT, "r");
  if (!file) {
    perror("fopen");
    return -1;
  }

  while (fgets(line, sizeof(line), file)) {
    if (sscanf(line, "pswpin %lu", &pagesSwappedIn) == 1 &&
        sscanf(line, "pswpout %lu", &pagesSwappedOut) == 1) {
      break;
    }
  }

  fclose(file);

  swapActivity = (double)pagesSwappedIn + pagesSwappedOut;
  return swapActivity;
}

void get_load_average(double loadavg[3]) {
  const char *const LOADAVG_FILE = "/proc/loadavg";
  FILE *file;

  file = fopen(LOADAVG_FILE, "r");
  if (!file) {
    perror("fopen");
    exit(EXIT_FAILURE);
  }

  if (fscanf(file, "%lf %lf %lf", &loadavg[0], &loadavg[1], &loadavg[2]) != 3) {
    perror("fscanf");
    exit(EXIT_FAILURE);
  }

  fclose(file);
}

double disk_io_activity() {
  const char *const PROC_DISKSTATS = "/proc/diskstats";
  FILE *file;
  char line[1024];
  char device[32];
  uint64_t readIO, writeIO, discardIO, flushIO;
  double iops;

  const char *targetDevice = "sda";

  file = fopen(PROC_DISKSTATS, "r");
  if (!file) {
    perror("fopen");
    return -1;
  }

  while (fgets(line, sizeof(line), file)) {

    if (sscanf(line,
               "%*u %*u %s %*u %*u %lu %*u %*u %lu %*u %*u %lu %*u %*u %*u %*u "
               "%lu %*u %*u %*u",
               device, &readIO, &writeIO, &discardIO, &flushIO) == 5) {
      if (strcmp(device, targetDevice) == 0) {
        break;
      }
    }
  }

  fclose(file);

  iops = ((double)readIO + writeIO + discardIO + flushIO) / 2.0;
  return iops;
}

double disk_io_rate() {
  const char *const PROC_DISKSTATS = "/proc/diskstats";
  char line[1024];
  FILE *file;
  char device[32];
  uint64_t readIO, writeIO;
  double ioRate;

  /* Specify the disk device you want to monitor (e.g., "sda") */
  const char *targetDevice = "sda";

  file = fopen(PROC_DISKSTATS, "r");
  if (!file) {
    perror("fopen");
    return -1;
  }

  while (fgets(line, sizeof(line), file)) {
    if (sscanf(line, "%*u %*u %s %*u %*u %lu %*u %*u %*u %lu", device, &readIO,
               &writeIO) == 3) {
      if (strcmp(device, targetDevice) == 0) {
        break;
      }
    }
  }

  fclose(file);

  /* Calculate I/O rate in kilobytes per second */
  ioRate = ((double)readIO + writeIO) / 1024.0;
  return ioRate;
}

/* calculate memory util */
double memory_util() {
  const char *const PROC_MEMINFO = "/proc/meminfo";
  char line[1024];
  FILE *file;
  unsigned long memTotal = 0, memFree = 0;
  double memUsage = 0.0;

  file = fopen(PROC_MEMINFO, "r");
  if (!file) {
    perror("fopen");
    return -1;
  }

  while (fgets(line, sizeof(line), file)) {
    if (strncmp(line, "MemTotal:", 9) == 0) {
      sscanf(line, "MemTotal: %lu kB", &memTotal);
    }
    if (strncmp(line, "MemFree:", 8) == 0) {
      sscanf(line, "MemFree: %lu kB", &memFree);
      break;
    }
  }

  fclose(file);

  if (memTotal > 0) {
    memUsage = 100.0 - ((memFree / (double)memTotal) * 100.0);
  }

  return memUsage;
}

void os_uptime() {
  struct sysinfo si;
  unsigned long days, hours, minutes, seconds;

  if (sysinfo(&si) != 0) {
    perror("sysinfo");
    exit(EXIT_FAILURE);
  }

  days = si.uptime / (60 * 60 * 24);
  hours = (si.uptime / (60 * 60)) % 24;
  minutes = (si.uptime / 60) % 60;
  seconds = si.uptime % 60;

  printf("OS Uptime: %lu days, %lu:%02lu:%02lu\n", days, hours, minutes,
         seconds);
}

int main(int argc, char *argv[]) {
  UNUSED(argc);
  UNUSED(argv);

  if (SIG_ERR == signal(SIGINT, _signal_)) {
    TRACE("signal()");
    return -1;
  }
  while (!done) {
    double loadavg[3];
    double ioRate, ioActivity, swapAct;

    printf("\033[2J\033[H");

    printf("--------\n");
    printf("CPU Utilization Metrics\n");

    printf("CPU Usage: %5.1f%%\n", get_cpu_util());

    get_load_average(loadavg);
    printf("Load Average (1min, 5min, 15min): %.2f, %.2f, %.2f\n", loadavg[0],
           loadavg[1], loadavg[2]);

    printf("--------\n");
    printf("I/O Metrics\n");

    ioRate = disk_io_rate();
    printf("Disk I/O Rate: %.2f KB/s\n", ioRate);

    ioActivity = disk_io_activity();
    printf("Disk I/O Activity (IOPS): %.2f\n", ioActivity);

    printf("--------\n");
    printf("Memory Metrics\n");

    printf("Memory Usage: %.2f%%\n", memory_util());

    swapAct = swap_activity();
    printf("Swap Activity: %.2f pages per second\n", swapAct);

    printf("--------\n");
    printf("System Uptime Metrics\n");

    os_uptime();

    printf("--------\n");
    fflush(stdout);
    us_sleep(500000);
  }

  printf("\rDone!   \n");
  return 0;
}
