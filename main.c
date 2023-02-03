#include <gpiod.h>
#include <stdio.h>
#include <unistd.h>
#include <stdint.h>
#include <inttypes.h>
#include <math.h>
#include <string.h>
#include <sched.h>

#ifndef	CONSUMER
#define	CONSUMER         "IR"
#endif

// #define SNIFF

#define IR_PIN           34
#define POWER_STATE_PIN  132
#define CHIP_NAME        "gpiochip0"

#define VOLUME_FILE      "/home/chip/ir/volume.txt"
#define MAX_VOLUME       30
#define DEFAULT_VOLUME   15

#define OFFSET           53
#define DELAY_LEAD       2400 - OFFSET
#define DELAY_BURST      600  - OFFSET
#define DELAY_0          600  - OFFSET
#define DELAY_1          1200 - OFFSET
#define DELAY_END        20000
#define DELAY_REPEAT     70000

#define ADDRESS          48

// Commands
#define VOLUME_UP        18
#define VOLUME_DOWN      19
#define POWER            21
#define VIDEO_1          34
#define SC               66

#define SNIFF_BUFFER     4096*10


static struct gpiod_chip *chip;
static struct gpiod_line *ir_line;
static struct gpiod_line *power_state_line;

#define SEND_0() {                           \
          gpiod_line_set_value(ir_line, 0);  \
          usleep(DELAY_0);                   \
          gpiod_line_set_value(ir_line, 1);  \
          usleep(DELAY_BURST);}

#define SEND_1() {                           \
          gpiod_line_set_value(ir_line, 0);  \
          usleep(DELAY_1);                   \
          gpiod_line_set_value(ir_line, 1);  \
          usleep(DELAY_BURST);}

void send(uint8_t command, uint8_t address) {
    for (uint_fast8_t i = 0; i < 3; i++) {
        gpiod_line_set_value(ir_line, 0);
        usleep(DELAY_LEAD);
        gpiod_line_set_value(ir_line, 1);
        usleep(DELAY_BURST);

        for (uint_fast8_t bits = 0; bits < 7; bits++) {
            if (command & (1 << bits)) {
                SEND_1();
            } else {
                SEND_0();
            }
        }

        for (uint_fast8_t bits = 0; bits < 8; bits++) {
            if (address & (1 << bits)) {
                SEND_1();
            } else {
                SEND_0();
            }
        }

        usleep(DELAY_END);
    }
}

void sniff(void) {
    while (gpiod_line_get_value(ir_line) == 1) {}

    int out[SNIFF_BUFFER];
    for (uint16_t i = 0; i < SNIFF_BUFFER; i++) {
        out[i] = gpiod_line_get_value(ir_line);
    }

    for (uint16_t i = 0; i < SNIFF_BUFFER; i++) {
        printf("%d, ", out[i]);
        if ((i+1) % 8 == 0) {
            printf("\n");
        }
    }

}

static void save_volume(uint8_t volume) {
    FILE *fp = fopen(VOLUME_FILE, "w");
    if (fp == NULL) {
        return;
    }
    char tmp[20];
    snprintf(tmp, 20, "%d", volume);
    fputs(tmp, fp);
    fclose(fp);
}

static uint8_t read_volume(void) {
    uint8_t volume = DEFAULT_VOLUME;
    FILE *fp = fopen(VOLUME_FILE, "r");
    if (fp != NULL) {
        fscanf(fp, "%" PRIu8, &volume);
        fclose(fp);
    }
    return volume;
}

void set_volume(uint8_t volume) {
    if (volume > MAX_VOLUME) {
        return;
    }

    uint8_t current_volume = read_volume();
    if (volume == current_volume) {
        return;
    }

    if (gpiod_line_get_value(power_state_line) == 0) {
        printf("Device is off\n");
        return;
    }

    uint8_t volume_diff = abs(volume - current_volume);
    uint8_t command = (volume > current_volume) ? VOLUME_UP : VOLUME_DOWN;

    for (uint_fast8_t i = 0; i < volume_diff; i++) {
        send(command, ADDRESS);
        usleep(DELAY_REPEAT);
    }

    save_volume(volume);
}

void power(bool value) {
    if (gpiod_line_get_value(power_state_line) != (int)value) {
        send(POWER, ADDRESS);
    }
    sleep(5);
}

bool set_realtime(void) {
    struct sched_param param;
    int policy;
    pid_t pid = getpid();

    policy = SCHED_FIFO;
    param.sched_priority = 99;

    if (sched_setscheduler(pid, policy, &param) == -1) {
        perror("sched_setscheduler failed\n");
        return false;
    }
    return true;
}

int main(int argc, char **argv) {
#if !defined(SNIFF)
    setuid(0);

    // run with real time scheduling
    if (!set_realtime()) {
        return 1;
    }

    if (argc < 3) {
        perror("Please provide two arguments when executing the program.\n");
        return 1;
    }

    float value;
    char *endptr;
    value = (float) strtof(argv[2], &endptr);
    if (*endptr) {
        perror("Argument 2 is not a valid number\n");
        return 1;
    }
#endif

    chip = gpiod_chip_open_by_name(CHIP_NAME);
    if (!chip) {
        perror("Open chip failed\n");
        goto end;
    }

    ir_line = gpiod_chip_get_line(chip, IR_PIN);
    if (!ir_line) {
        perror("Get ir line failed\n");
        goto close_chip;
    }

    power_state_line = gpiod_chip_get_line(chip, POWER_STATE_PIN);
    if (!power_state_line) {
        perror("Get power state line failed\n");
        goto close_chip;
    }

#if defined(SNIFF)
    int ret = gpiod_line_request_input(ir_line, CONSUMER);
#else
    int ret = gpiod_line_request_output(ir_line, CONSUMER, 1);
#endif
    if (ret < 0) {
        perror("Request line failed\n");
        goto release_line;
    }

#if defined(SNIFF)
    sniff();
#else

    ret = gpiod_line_request_input(power_state_line, CONSUMER);
    if (ret < 0) {
        perror("Request power state line failed\n");
        goto release_line;
    }

    // raw volume
    if (!strcmp(argv[1], "vol")) {
        printf("Set volume to: %d\n", (int) value);
        set_volume(value);
    // power command
    } else if (!strcmp(argv[1], "power")) {
        printf("Power: %d\n", (int) value);
        power(value);
    } else if (!strcmp(argv[1], "shair_vol")) {
        // convert airplay volume to raw value
        int raw = (30 + value)*MAX_VOLUME/30;
        if (raw < 0) {
            raw = 0;
        }
        printf("Shairport-sync volume: %f, %d\n", value, raw);
        set_volume(raw);
    }

    gpiod_line_release(ir_line);
    ret = gpiod_line_request_input(ir_line, CONSUMER);
    if (ret < 0) {
        perror("Request ir line as input failed\n");
        goto release_line;
    }

#endif

release_line:
    gpiod_line_release(ir_line);
    gpiod_line_release(power_state_line);
close_chip:
    gpiod_chip_close(chip);
end:
    return 0;
}
