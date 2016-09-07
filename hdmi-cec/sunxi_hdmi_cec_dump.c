#define LOG_TAG "test"

#include <hardware/hdmi_cec.h>

#include <stdlib.h>
#include <pthread.h>
#include <stdio.h>
#include <android/log.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "log.h"

#define BASE_ADDRESS (unsigned long long)0xffffff8002600000
#define CEC_PHY_ADDRESS 0x1003c

#define DUMP_REG "/sys/devices/virtual/misc/sunxi-reg/rw/dump"

static int fd;

struct timespec current_time()
{
    struct timespec start_time;
    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &start_time);
    return start_time;
}

long long timer_diff(struct timespec start_time, struct timespec end_time)
{
    long long diff = end_time.tv_nsec - start_time.tv_nsec;
    if (end_time.tv_sec > start_time.tv_sec) {
      diff += (end_time.tv_sec - start_time.tv_sec) * 1000LL * 1000LL * 1000LL;
    }
    return diff;
}

// #define PRINT_STREAM
#define WAIT_TIME 80
#define IN_LINE 80
#define START_BIT 10
#define BIT_1_HI 4
#define BIT_1_LO 2
#define BIT_0_HI 2
#define BIT_0_LO 4

void sleep_us(struct timespec start_time, long long how_long)
{
  for(;;) {
    if(timer_diff(start_time, current_time()) > how_long * 1000LL)
      break;
    usleep(1);
  }
}

int bit_count = -1;
int bit_value = 0;
int bit_index = 0;

void start_bit() {
  bit_count = 0;
  bit_value = 0;
  bit_index = 0;
}

void add_bit(int bit) {
  if(bit_count < 0) {
    return;
  }

  bit_count++;
  bit_value <<= 1;
  bit_value |= bit ? 1 : 0;

  if(bit_count == 10) {
    fprintf(stdout, "%d %02x EOM:%d ACK:%d\n", bit_index, bit_value >> 2, (bit_value & 2) ? 1 : 0, ~bit_value & 1);
    fflush(stdout);
    if(bit_value & 2) {
      bit_count = -1;
    } else {
      bit_count = 0;
      bit_value = 0;
      bit_index++;
    }
  }
}

int main(int argc, char *argv[])
{
  char line[IN_LINE+1] = {0};
  char buffer[64];
  char decoded[64];
  int ret;
  struct timespec last, current;

  int hi = 0, lo = 0, last_hi = 0, last_lo = 0, hilo = -1;

  fd = open(DUMP_REG, O_RDWR);
  if(fd < 0) {
    perror("Failed to open sunxi-reg/dump");
    return 1;
  }

  sprintf(buffer, "0x%016llx,0x%016llx", BASE_ADDRESS + CEC_PHY_ADDRESS, BASE_ADDRESS + CEC_PHY_ADDRESS);
  printf("Requesting: %s\n", buffer);

  ret = write(fd, buffer, strlen(buffer));
  if(ret < 0) {
    perror("Failed to request CEC_PHY");
    return 1;
  }

  int count = 0;

  last = current_time();

  for(;;) {
    unsigned int value;

    struct timespec start = current_time();

    lseek(fd, 0, SEEK_SET);

    ret = read(fd, buffer, sizeof(buffer)-1);
    if(ret < 0) {
      perror("Failed to read CEC_PHY");
      return 1;
    }

    buffer[ret] = 0;

    ret = sscanf(buffer, "0x%08x", &value);
    if(ret != 1) {
      fprintf(stderr, "Failed to parse: %s", buffer);
      return 1;
    }

    switch(value) {
    case 0x2: // LINE ON
      line[count] = 'X';
      if(hi == 0) {
        last_hi = hi;
        last_lo = lo;
        lo = 0;
      }
      hi++;
      break;

    case 0x4:
      line[count] = 'Y';
      break;

    case 0x0: // LINE OFF
      line[count] = '.';
      if(lo == 0) {
        last_hi = hi;
        last_lo = lo;
        hi = 0;
      }
      lo++;
      break;

    case 0x84:
      line[count] = ':';
      break;

    case 0x86:
      line[count] = ';';
      break;

    default:
      line[count] = '+';
      break;
    }

    if(lo > START_BIT) {
      last_hi = last_lo = lo = hi = 0;
      strcat(decoded, "S");
      start_bit();
    } else if(last_lo >= BIT_1_LO && hi >= BIT_1_HI) {
      last_hi = last_lo = lo = hi = 0;
      strcat(decoded, "1");
      add_bit(1);
    } else if(last_lo >= BIT_0_LO && hi >= BIT_0_HI) {
      last_hi = last_lo = lo = hi = 0;
      strcat(decoded, "0");
      add_bit(0);
    }

    sleep_us(start, WAIT_TIME);

    if(++count >= IN_LINE) {
      last = current;
      current = current_time();

#ifdef PRINT_STREAM
      long long diff = timer_diff(last, current);
      fprintf(stdout, "%s: took %lldus: %s", line, diff / 1000 / count, decoded);
      fputc('\n', stdout);
      fflush(stdout);
#endif
      decoded[0] = 0;

      count = 0;
    }
  }

  close(fd);
  return 0;
}
