#ifndef STEP_H
#define STEP_H

typedef struct {
        int i2c_fd;
        bool moving;
        int target_steps;
        int current_position;
        int target_position;
        int direction;
        volatile sig_atomic_t abort_requested;
        indigo_timer *timer;
        indigo_property *focuser_settings_property;
        indigo_property *step_size_property;
} adafruitmh_private_data;

int  stepper_init(void);
void stepper_close(int i2c_fd);
void stepper_focus(adafruitmh_private_data *private_data);

#endif
