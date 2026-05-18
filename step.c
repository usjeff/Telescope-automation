//# SPDX-FileCopyrightText: 2017 Scott Shawcroft for Adafruit Industries
//# SPDX-FileCopyrightText: 2018 Kattni Rembor for Adafruit Industries
//#
//# SPDX-License-Identifier: MIT

/*
`adafruit_motorkit`
====================================================

CircuitPython helper library for DC & Stepper Motor FeatherWing, Shield, and Pi Hat kits.

* Author(s): Scott Shawcroft, Kattni Rembor

Implementation Notes
--------------------

**Hardware:**

* `DC Motor + Stepper FeatherWing <https://www.adafruit.com/product/2927>`_
* `Adafruit Motor/Stepper/Servo Shield for Arduino v2 Kit <https://www.adafruit.com/product/1438>`_
* `Adafruit DC & Stepper Motor HAT for Raspberry Pi - Mini Kit
  <https://www.adafruit.com/product/2348>`_

**Software and Dependencies:**

* Adafruit CircuitPython firmware for the supported boards:
  https://github.com/adafruit/circuitpython/releases

 * Adafruit's Bus Device library: https://github.com/adafruit/Adafruit_CircuitPython_BusDevice
 * Adafruit's Register library: https://github.com/adafruit/Adafruit_CircuitPython_Register
 * Adafruit's PCA9685 library: https://github.com/adafruit/Adafruit_CircuitPython_PCA9685
 * Adafruit's Motor library: https://github.com/adafruit/Adafruit_CircuitPython_Motor

 This file, step.c, is based on the python code from Adafruit as above. JeffD

 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/i2c-dev.h>
#include <math.h>

#include <signal.h>

#include <indigo/indigo_io.h>
#include <indigo/indigo_focuser_driver.h>

#include "step.h"

// PCA9685 Registers
#define PCA9685_ADDRESS 0x60
#define MODE1 0x00
#define PRESCALE 0xFE
#define LED0_ON_L 0x06

// Stepper 1 Pin Mapping for Adafruit Motor HAT V2
#define AIN1 10
#define AIN2 9
#define BIN1 11
#define BIN2 12
#define PWMA 8
#define PWMB 13

void i2c_write_byte(int i2c_fd, uint8_t reg, uint8_t val);
void set_pwm(int i2c_fd, uint8_t channel, uint16_t on, uint16_t off);
void set_pin(int i2c_fd, uint8_t pin, uint8_t value);

int stepper_init(void);
void stepper_close(int i2c_fd);
void release(int i2c_fd);

// volatile sig_atomic_t abort_requested;

// int i2c_fd; moved to private data.

// Helper to write a byte to a specific register
void i2c_write_byte(int i2c_fd, uint8_t reg, uint8_t val) {
    uint8_t buf[2] = {reg, val};
    if (write(i2c_fd, buf, 2) != 2) {
        perror("Failed to write to I2C bus");
    }
}

// Set PWM duty cycle for a specific channel
// on: 0-4095, off: 0-4095
void set_pwm(int i2c_fd, uint8_t channel, uint16_t on, uint16_t off) {
    i2c_write_byte(i2c_fd, LED0_ON_L + 4 * channel, on & 0xFF);
    i2c_write_byte(i2c_fd, LED0_ON_L + 4 * channel + 1, on >> 8);
    i2c_write_byte(i2c_fd, LED0_ON_L + 4 * channel + 2, off & 0xFF);
    i2c_write_byte(i2c_fd, LED0_ON_L + 4 * channel + 3, off >> 8);
}

// Helper to set a pin fully ON or fully OFF
void set_pin(int i2c_fd, uint8_t pin, uint8_t value) {
    if (value == 1) {
        set_pwm(i2c_fd, pin, 4096, 0); // Fully ON
    } else {
        set_pwm(i2c_fd, pin, 0, 4096); // Fully OFF
    }
}

// Initialize PCA9685
int stepper_init(void) {
    int i2c_fd = open("/dev/i2c-1", O_RDWR);
    if (i2c_fd < 0) {
        perror("I2C open failed");
        return(-1);
    }

    if (ioctl(i2c_fd, I2C_SLAVE, PCA9685_ADDRESS) < 0) {
        perror("I2C slave select failed");
        return(-1);
    }

    // Reset PCA9685
    i2c_write_byte(i2c_fd, MODE1, 0x00);
    usleep(10000);

    //abort_requested = 0;
    return(i2c_fd);
}

void stepper_close(int i2c_fd) {
    release(i2c_fd);
    if (i2c_fd > 0) {
        close(i2c_fd);
        i2c_fd = -1;
    }
}

// Release the motor (de-energize coils)
void release(int i2c_fd) {
    set_pin(i2c_fd, AIN1, 0);
    set_pin(i2c_fd, AIN2, 0);
    set_pin(i2c_fd, BIN1, 0);
    set_pin(i2c_fd, BIN2, 0);
    set_pin(i2c_fd, PWMA, 0);
    set_pin(i2c_fd, PWMB, 0);
}

// Global step index to track position in the sequence
int current_step = 0;

// Perform a single step in DOUBLE style
void onestep(int i2c_fd, int direction) {
    // Double step sequence (2 coils on at a time for max torque)
    // Step 0: AIN1, BIN1
    // Step 1: AIN2, BIN1
    // Step 2: AIN2, BIN2
    // Step 3: AIN1, BIN2
    
    if (direction > 0) { // FORWARD
        current_step++;
    } else { // BACKWARD
        current_step--;
    }

    // Wrap index between 0 and 3
    int step = abs(current_step) % 4;
    if (current_step < 0 && step != 0) step = 4 - step;

    // Set PWM pins to full power
    set_pin(i2c_fd, PWMA, 1);
    set_pin(i2c_fd, PWMB, 1);

    switch (step) {
        case 0:
            set_pin(i2c_fd, AIN1, 1); set_pin(i2c_fd, AIN2, 0); set_pin(i2c_fd, BIN1, 1); set_pin(i2c_fd, BIN2, 0);
            break;
        case 1:
            set_pin(i2c_fd, AIN1, 0); set_pin(i2c_fd, AIN2, 1); set_pin(i2c_fd, BIN1, 1); set_pin(i2c_fd, BIN2, 0);
            break;
        case 2:
            set_pin(i2c_fd, AIN1, 0); set_pin(i2c_fd, AIN2, 1); set_pin(i2c_fd, BIN1, 0); set_pin(i2c_fd, BIN2, 1);
            break;
        case 3:
            set_pin(i2c_fd, AIN1, 1); set_pin(i2c_fd, AIN2, 0); set_pin(i2c_fd, BIN1, 0); set_pin(i2c_fd, BIN2, 1);
            break;
    }
}

void stepper_focus(adafruitmh_private_data *data) {
    int i2c_fd = data->i2c_fd;
    if (i2c_fd <= 0)
        return;

    int count = data->target_steps;
    int loop = abs(count);

    for (int i = 0; i < loop; i++) {
        if (data->abort_requested)
            break;

        if (data->i2c_fd <= 0)
            break;

        if (count > 0) {
            onestep(i2c_fd, 1); // FORWARD
        } else if (count < 0) {
            onestep(i2c_fd, -1); // BACKWARD
        }
 
        // 0.01 seconds = 10,000 microseconds
        usleep(10000);
    }

    release(i2c_fd);

}

