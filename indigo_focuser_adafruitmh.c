// Copyright (c) 2025 Jeff Daniels
// All rights reserved.
//
// You can use this software under the terms of 'INDIGO Astronomy
// open-source license' (see LICENSE.md).
//
// THIS SOFTWARE IS PROVIDED BY THE AUTHORS 'AS IS' AND ANY EXPRESS
// OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
// WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
// DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
// GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
// WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
// NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
// SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

// version history
// 1.0 by Jeff Daniels 

/** INDIGO AdaFruit Motor Hat focuser driver
 \file indigo_focuser_adafruitmh.c
 */

#define DRIVER_VERSION 0x0001
#define DRIVER_NAME "indigo_focuser_adafruitmh"

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <math.h>
#include <assert.h>
#include <errno.h>
#include <stdarg.h>
#include <signal.h>


#include <sys/time.h>

#include <indigo/indigo_driver_xml.h>
#include <indigo/indigo_io.h>
#include <indigo/indigo_focuser_driver.h>

#include "indigo_focuser_adafruitmh.h"
#include "step.h"

#define REQUIRE_PRIVATE_DATA() if ((!device) || (!device->private_data)) return INDIGO_OK;

#define PRIVATE_DATA		((adafruitmh_private_data *)device->private_data)

#define FOCUSER_SETTINGS_PROPERTY                                       (PRIVATE_DATA->focuser_settings_property)
#define FOCUSER_SETTINGS_FOCUS_ITEM                                     (FOCUSER_SETTINGS_PROPERTY->items+0)
#define FOCUSER_SETTINGS_BL_ITEM                                        (FOCUSER_SETTINGS_PROPERTY->items+1)  

indigo_result indigo_focuser_adafruitmh(indigo_driver_action action, indigo_driver_info *info);
 
static indigo_result focuser_attach(indigo_device *device);
static indigo_result focuser_detach(indigo_device *device);
static indigo_result focuser_enumerate_properties(indigo_device *device, indigo_client *client, indigo_property *property);
static indigo_result focuser_change_property(indigo_device *device, indigo_client *client, indigo_property *property);

static indigo_device focuser_device;
#if 0
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
#endif

// -------------------------------------------------------------------------------- INDIGO focuser device implementation
static void focuser_detach_callback(indigo_device *device) {
        if ((!device) || (!device->private_data))
            return;

        INDIGO_DRIVER_LOG(DRIVER_NAME, "focuser_detach_callback fired");

        adafruitmh_private_data *data = PRIVATE_DATA;

        if ((!IS_CONNECTED) || (data->i2c_fd <= 0))
             goto finish;

        if (data->moving) {
            data->abort_requested = true;
            sleep(5);
        }

//        data->moving = false;
//        data->abort_requested = false;
#if 0
        indigo_delete_property(device, FOCUSER_STEPS_PROPERTY, NULL);
        indigo_delete_property(device, FOCUSER_POSITION_PROPERTY, NULL);
        indigo_delete_property(device, FOCUSER_ABORT_MOTION_PROPERTY, NULL);
        indigo_delete_property(device, FOCUSER_DIRECTION_PROPERTY, NULL);
        indigo_delete_property(device, FOCUSER_SPEED_PROPERTY, NULL);
        indigo_delete_property(device, FOCUSER_SETTINGS_PROPERTY, NULL);
        stepper_close(PRIVATE_DATA->i2c_fd);

        free(device->private_data);
        device->private_data = NULL;
#endif
finish:
//        FOCUSER_ABORT_MOTION_PROPERTY->state = INDIGO_OK_STATE;
//        indigo_update_property(device, FOCUSER_ABORT_MOTION_PROPERTY, NULL);
}

static void focuser_abort_callback(indigo_device *device) {
        if ((!device) || (!device->private_data))
            return;

        INDIGO_DRIVER_LOG(DRIVER_NAME, "focuser_abort_callback fired");

        adafruitmh_private_data *data = PRIVATE_DATA;

        if ((!IS_CONNECTED) || (data->i2c_fd <= 0))
             goto finish;

        if (data->moving)
             sleep(1);

        data->moving = false;
        data->abort_requested = false;

finish:
        FOCUSER_ABORT_MOTION_PROPERTY->state = INDIGO_OK_STATE;
        indigo_update_property(device, FOCUSER_ABORT_MOTION_PROPERTY, NULL);
}

static void focuser_steps_callback(indigo_device *device) {
        if ((!device) || (!device->private_data))
            return;

        //INDIGO_DRIVER_LOG(DRIVER_NAME, "focuser_steps_callback fired");

        adafruitmh_private_data *data = PRIVATE_DATA;

        if ((!IS_CONNECTED) || (data->i2c_fd < 0))
             goto finish;

        INDIGO_DRIVER_LOG(DRIVER_NAME, "steps_callback: Async move %d steps", data->target_steps);

        if (data->abort_requested) {
            data->moving = false;
            goto finish;
        }

        // Blocking call is now safe -- we're off the property thread
        stepper_focus(data);
        data->current_position += data->target_steps;
        data->moving = false;

        FOCUSER_POSITION_ITEM->number.value = data->current_position;
        indigo_update_property(device, FOCUSER_POSITION_PROPERTY, NULL);

finish:
        FOCUSER_STEPS_PROPERTY->state = INDIGO_OK_STATE;
        indigo_update_property(device, FOCUSER_STEPS_PROPERTY, NULL);
}

static void focuser_speed_callback(indigo_device *device) {
        if ((!device) || (!device->private_data))
            return;

        //INDIGO_DRIVER_LOG(DRIVER_NAME, "focuser_speed_callback fired");

        adafruitmh_private_data *data = PRIVATE_DATA;

        if ((!IS_CONNECTED) || (data->i2c_fd < 0))
             goto finish;

        //INDIGO_DRIVER_LOG(DRIVER_NAME, "speed_callback: Async move %d steps", data->target_steps);

        if (data->abort_requested) {
            data->moving = false;
            goto finish;
        }

        // Blocking call is now safe -- we're off the property thread
        stepper_focus(data);
        data->current_position += data->target_steps;
        data->moving = false;

        FOCUSER_POSITION_ITEM->number.value = data->current_position;
        FOCUSER_POSITION_PROPERTY->state = INDIGO_OK_STATE;
        indigo_update_property(device, FOCUSER_POSITION_PROPERTY, NULL);

finish:
        FOCUSER_SPEED_PROPERTY->state = INDIGO_OK_STATE;
        indigo_update_property(device, FOCUSER_SPEED_PROPERTY, NULL);
}

static indigo_result focuser_attach(indigo_device *device) {

        if (!device)
           return INDIGO_OK;

        device->private_data = calloc(1, sizeof(adafruitmh_private_data));

        if (!device->private_data)
           return INDIGO_FAILED;

        PRIVATE_DATA->i2c_fd = -1;
        indigo_result result = indigo_focuser_attach(device, DRIVER_NAME, DRIVER_VERSION);

        if (result != INDIGO_OK)
          return result;

        // Declare Driver Capabilities
        FOCUSER_SETTINGS_PROPERTY = indigo_init_number_property(
              NULL,
              device->name,
              "FOCUSER_SETUP",
              MAIN_GROUP,
              "Focuser Setup",
              INDIGO_OK_STATE,
              INDIGO_RW_PERM,
              2
        );

        indigo_init_number_item(FOCUSER_SETTINGS_FOCUS_ITEM,
                               "FOCUS",
                               "Focus ",
                               FOCUSER_POSITION_ITEM->number.min,
                               FOCUSER_POSITION_ITEM->number.max,
                               FOCUSER_POSITION_ITEM->number.value,
                               0
        );

        FOCUSER_STEPS_PROPERTY->perm = INDIGO_RW_PERM;
        FOCUSER_STEPS_PROPERTY->state = INDIGO_OK_STATE;
        FOCUSER_STEPS_ITEM->number.step = 0;
        FOCUSER_STEPS_ITEM->number.target = 0; 
        FOCUSER_STEPS_ITEM->number.value = 0;
        FOCUSER_STEPS_ITEM->number.min = -10000;
        FOCUSER_STEPS_ITEM->number.max = 10000;

        FOCUSER_SPEED_PROPERTY->perm = INDIGO_RW_PERM;
        FOCUSER_SPEED_PROPERTY->state = INDIGO_OK_STATE;
        FOCUSER_SPEED_ITEM->number.step  = 0;
        FOCUSER_SPEED_ITEM->number.target = 0; 
        FOCUSER_SPEED_ITEM->number.value = 0;
        FOCUSER_SPEED_ITEM->number.min = -5000;
        FOCUSER_SPEED_ITEM->number.max = 5000;

        FOCUSER_POSITION_PROPERTY->perm = INDIGO_RW_PERM;
        FOCUSER_POSITION_PROPERTY->state = INDIGO_OK_STATE;
        FOCUSER_POSITION_ITEM->number.step = 0;
        FOCUSER_POSITION_ITEM->number.target = 0;
        FOCUSER_POSITION_ITEM->number.value = 0;
        FOCUSER_POSITION_ITEM->number.min = -5000;
        FOCUSER_POSITION_ITEM->number.max = 5000;

        FOCUSER_DIRECTION_PROPERTY->perm = INDIGO_RW_PERM;
        FOCUSER_DIRECTION_PROPERTY->state = INDIGO_OK_STATE;
        FOCUSER_DIRECTION_MOVE_INWARD_ITEM->sw.value = 0;
        FOCUSER_DIRECTION_MOVE_OUTWARD_ITEM->sw.value = 1;

        DEVICE_PORT_PROPERTY->hidden = true;
        DEVICE_PORTS_PROPERTY->hidden = true;
        FOCUSER_REVERSE_MOTION_PROPERTY->hidden = true;

        FOCUSER_ABORT_MOTION_PROPERTY->hidden = false;
        FOCUSER_ABORT_MOTION_PROPERTY->perm = INDIGO_RW_PERM;
        FOCUSER_ABORT_MOTION_PROPERTY->state = INDIGO_OK_STATE;

        //INDIGO_DRIVER_LOG(DRIVER_NAME, "Step size property created");
        //INDIGO_DRIVER_LOG(DRIVER_NAME, "Focuser_Steps perm=%d state=%d", FOCUSER_STEPS_PROPERTY->perm, FOCUSER_STEPS_PROPERTY->state);
        //INDIGO_DRIVER_LOG(DRIVER_NAME, "Focuser_Position perm=%d state=%d", FOCUSER_POSITION_PROPERTY->perm, FOCUSER_POSITION_PROPERTY->state);
        INDIGO_DRIVER_LOG(DRIVER_NAME, "Adafruit Motor Hat initialized !");
        return focuser_enumerate_properties(device, NULL, NULL);
}

static indigo_result focuser_detach(indigo_device *device) {
        REQUIRE_PRIVATE_DATA()

        if (PRIVATE_DATA->moving) {
            indigo_cancel_timer(device, &PRIVATE_DATA->timer);
            PRIVATE_DATA->abort_requested = true;
//            indigo_set_timer(device, 0, focuser_detach_callback, &PRIVATE_DATA->timer);
//            INDIGO_DRIVER_LOG(DRIVER_NAME, "Adafruit Motor Hat waiting");
//            sleep(10);
        }

        if (PRIVATE_DATA->i2c_fd >= 0) {
            indigo_delete_property(device, FOCUSER_STEPS_PROPERTY, NULL);
            indigo_delete_property(device, FOCUSER_POSITION_PROPERTY, NULL);
            indigo_delete_property(device, FOCUSER_ABORT_MOTION_PROPERTY, NULL);
            indigo_delete_property(device, FOCUSER_DIRECTION_PROPERTY, NULL);
            indigo_delete_property(device, FOCUSER_SPEED_PROPERTY, NULL);
            indigo_delete_property(device, FOCUSER_SETTINGS_PROPERTY, NULL);
            stepper_close(PRIVATE_DATA->i2c_fd);
         }

        free(device->private_data);
        device->private_data = NULL;

        INDIGO_DRIVER_LOG(DRIVER_NAME, "Adafruit Motor Hat closed");
        return indigo_focuser_detach(device);
}

static indigo_result focuser_enumerate_properties(indigo_device *device, indigo_client *client, indigo_property *property) {
        REQUIRE_PRIVATE_DATA()

        //if (IS_CONNECTED)
        {
            INDIGO_DRIVER_LOG(DRIVER_NAME, "In focuser_enumerate_properties");
            indigo_define_matching_property(FOCUSER_STEPS_PROPERTY);
            indigo_define_matching_property(FOCUSER_POSITION_PROPERTY);
            indigo_define_matching_property(FOCUSER_ABORT_MOTION_PROPERTY);
            indigo_define_matching_property(FOCUSER_DIRECTION_PROPERTY);
            indigo_define_matching_property(FOCUSER_SPEED_PROPERTY);
            indigo_define_matching_property(FOCUSER_SETTINGS_PROPERTY);
         }

        INDIGO_DRIVER_LOG(DRIVER_NAME, "Enumerate: POS=%p STEPS=%p", FOCUSER_POSITION_PROPERTY, FOCUSER_STEPS_PROPERTY);
	return indigo_focuser_enumerate_properties(device, client, property);
}

// --------------------- Change Property ---------------------
static indigo_result focuser_change_property(indigo_device *device, indigo_client *client, indigo_property *property) 
{
    REQUIRE_PRIVATE_DATA()

    INDIGO_DRIVER_LOG(DRIVER_NAME, "Focuser change property: %s", property->name);

    if (indigo_property_match_changeable(CONNECTION_PROPERTY, property)) {

        indigo_property_copy_values(CONNECTION_PROPERTY, property, false);

        INDIGO_DRIVER_LOG(DRIVER_NAME, "CONNECTION_CONNECTED_ITEM: %d", CONNECTION_CONNECTED_ITEM->sw.value);

        if (CONNECTION_CONNECTED_ITEM->sw.value) {
            PRIVATE_DATA->moving = false;
            PRIVATE_DATA->abort_requested = false;
            PRIVATE_DATA->i2c_fd = stepper_init();
            PRIVATE_DATA->current_position = 0;
            CONNECTION_PROPERTY->state = PRIVATE_DATA->i2c_fd >= 0 ? INDIGO_OK_STATE : INDIGO_ALERT_STATE;
#if 0
            if (PRIVATE_DATA->i2c_fd >= 0) {
               indigo_define_property(device, FOCUSER_STEPS_PROPERTY, NULL);
               indigo_define_property(device, FOCUSER_POSITION_PROPERTY, NULL);
               indigo_define_property(device, FOCUSER_ABORT_MOTION_PROPERTY, NULL);
               indigo_define_property(device, FOCUSER_DIRECTION_PROPERTY, NULL);
               indigo_define_property(device, FOCUSER_SPEED_PROPERTY, NULL);
            }
#endif
        } else {
            PRIVATE_DATA->abort_requested = true;
            sleep(2);

            if (PRIVATE_DATA->i2c_fd >= 0) {
                stepper_close(PRIVATE_DATA->i2c_fd);
                sleep(2);
                PRIVATE_DATA->abort_requested = false;
            }

            PRIVATE_DATA->abort_requested = false;
            PRIVATE_DATA->i2c_fd = -1;
            CONNECTION_PROPERTY->state = INDIGO_OK_STATE;
        }

        indigo_update_property(device, CONNECTION_PROPERTY, NULL);
        return INDIGO_OK;
    }

    if (indigo_property_match_changeable(FOCUSER_STEPS_PROPERTY, property)) {
           if ((!IS_CONNECTED) || (PRIVATE_DATA->i2c_fd < 0) || PRIVATE_DATA->moving) {
               return INDIGO_OK;
           }

           indigo_property_copy_values(FOCUSER_DIRECTION_PROPERTY, property, false);
           int direction = 1;
           if (FOCUSER_DIRECTION_MOVE_INWARD_ITEM->sw.value)
               direction = -1;
           FOCUSER_DIRECTION_PROPERTY->state = INDIGO_OK_STATE;
           indigo_update_property(device, FOCUSER_DIRECTION_PROPERTY, NULL);

           indigo_property_copy_values(FOCUSER_STEPS_PROPERTY, property, false);
           int steps = (int)FOCUSER_STEPS_ITEM->number.value;
           steps = steps * direction;

           PRIVATE_DATA->target_steps = steps;
           PRIVATE_DATA->moving = true;
           //INDIGO_DRIVER_LOG(DRIVER_NAME, "FOCUSER_STEPS_PROPERTY: move %d steps*dir", steps);
           FOCUSER_STEPS_PROPERTY->state = INDIGO_BUSY_STATE;
           indigo_update_property(device, FOCUSER_STEPS_PROPERTY, NULL);

           indigo_set_timer(device, 0, focuser_steps_callback, &PRIVATE_DATA->timer);
           return INDIGO_OK;
    }

    else if (indigo_property_match_changeable(FOCUSER_SPEED_PROPERTY, property)) {
           if ((!IS_CONNECTED) || (PRIVATE_DATA->i2c_fd < 0) || PRIVATE_DATA->moving) {
               return INDIGO_OK;
           }

           indigo_property_copy_values(FOCUSER_DIRECTION_PROPERTY, property, false);
           //INDIGO_DRIVER_LOG(DRIVER_NAME, "FOCUSER_DIRECTION_PROPERTY: %d in.value", FOCUSER_DIRECTION_MOVE_INWARD_ITEM->sw.value);
           //INDIGO_DRIVER_LOG(DRIVER_NAME, "FOCUSER_DIRECTION_PROPERTY: %d out.value", FOCUSER_DIRECTION_MOVE_OUTWARD_ITEM->sw.value);

           int direction = 1;
           if (FOCUSER_DIRECTION_MOVE_INWARD_ITEM->sw.value)
               direction = -1;
           FOCUSER_DIRECTION_PROPERTY->state = INDIGO_OK_STATE;
           indigo_update_property(device, FOCUSER_DIRECTION_PROPERTY, NULL);

           indigo_property_copy_values(FOCUSER_SPEED_PROPERTY, property, false);
           //INDIGO_DRIVER_LOG(DRIVER_NAME, "FOCUSER_SPEED_PROPERTY: move %d number.value", (int)FOCUSER_SPEED_ITEM->number.value);
           //INDIGO_DRIVER_LOG(DRIVER_NAME, "FOCUSER_SPEED_PROPERTY: move %d number.steps", (int)FOCUSER_SPEED_ITEM->number.step);
           //INDIGO_DRIVER_LOG(DRIVER_NAME, "FOCUSER_SPEED_PROPERTY: move %d number.target", (int)FOCUSER_SPEED_ITEM->number.target);

           int steps = (int)FOCUSER_SPEED_ITEM->number.value;
           steps = steps * direction;

           PRIVATE_DATA->target_steps = steps;
           PRIVATE_DATA->moving = true;
           //INDIGO_DRIVER_LOG(DRIVER_NAME, "FOCUSER_SPEED_PROPERTY: move %d steps*dir", steps);
           //INDIGO_DRIVER_LOG(DRIVER_NAME, "FOCUSER_SPEED_PROPERTY: move %d number.steps", (int)FOCUSER_SPEED_ITEM->number.step);
           //INDIGO_DRIVER_LOG(DRIVER_NAME, "FOCUSER_SPEED_PROPERTY: move %d number.target", (int)FOCUSER_SPEED_ITEM->number.target);

           FOCUSER_SPEED_PROPERTY->state = INDIGO_BUSY_STATE;
           indigo_update_property(device, FOCUSER_SPEED_PROPERTY, NULL);

           indigo_set_timer(device, 0, focuser_speed_callback, &PRIVATE_DATA->timer);
           return INDIGO_OK;
    }

    else if (indigo_property_match_changeable(FOCUSER_POSITION_PROPERTY, property)) {
           if ((!IS_CONNECTED) || (PRIVATE_DATA->i2c_fd < 0) || PRIVATE_DATA->moving) {
               return INDIGO_OK;
           }

           indigo_property_copy_values(FOCUSER_DIRECTION_PROPERTY, property, false);
           int direction = 1;
           if (FOCUSER_DIRECTION_MOVE_INWARD_ITEM->sw.value)
               direction = -1;
           FOCUSER_DIRECTION_PROPERTY->state = INDIGO_OK_STATE;
           indigo_update_property(device, FOCUSER_DIRECTION_PROPERTY, NULL);

           indigo_property_copy_values(FOCUSER_SPEED_PROPERTY, property, false);
           int steps = (int)FOCUSER_SPEED_ITEM->number.value;
           steps = steps * direction;
           indigo_property_copy_values(FOCUSER_SPEED_PROPERTY, property, false);

           int target = (int)FOCUSER_POSITION_ITEM->number.value;
           int delta  = target - PRIVATE_DATA->current_position;

           if (delta == 0)
              return INDIGO_OK;

           PRIVATE_DATA->target_steps = steps;
           PRIVATE_DATA->moving = true;
           INDIGO_DRIVER_LOG(DRIVER_NAME, "FOCUSER_POSITION_PROPERTY: Move %d steps", delta);
           FOCUSER_POSITION_PROPERTY->state = INDIGO_BUSY_STATE;
           indigo_update_property(device, FOCUSER_POSITION_PROPERTY, NULL);

           indigo_set_timer(device, 0, focuser_steps_callback, &PRIVATE_DATA->timer);
           return INDIGO_OK;
    }

    else if (indigo_property_match_changeable(FOCUSER_DIRECTION_PROPERTY, property)) {
           if ((!IS_CONNECTED) || (PRIVATE_DATA->i2c_fd < 0) || PRIVATE_DATA->moving) {
               return INDIGO_OK;
           }

           indigo_property_copy_values(FOCUSER_DIRECTION_PROPERTY, property, false);

           INDIGO_DRIVER_LOG(DRIVER_NAME, "FOCUSER_DIRECTION_MOVE_INWARD_ITEM:  %d", FOCUSER_DIRECTION_MOVE_INWARD_ITEM->sw.value);
           INDIGO_DRIVER_LOG(DRIVER_NAME, "FOCUSER_DIRECTION_MOVE_OUTWARD_ITEM:  %d", FOCUSER_DIRECTION_MOVE_OUTWARD_ITEM->sw.value);

           int direction = FOCUSER_DIRECTION_MOVE_INWARD_ITEM->sw.value ? -1 : 1;

           PRIVATE_DATA->direction = direction;

           FOCUSER_DIRECTION_PROPERTY->state = INDIGO_OK_STATE;
           indigo_update_property(device, FOCUSER_DIRECTION_PROPERTY, NULL);
           return INDIGO_OK;
    }

    else if (indigo_property_match_changeable(FOCUSER_ABORT_MOTION_PROPERTY, property)) {
           if ((!IS_CONNECTED) || (PRIVATE_DATA->i2c_fd <= 0)) {
               return INDIGO_OK;
           }

           PRIVATE_DATA->abort_requested = true;
           indigo_cancel_timer(device, &PRIVATE_DATA->timer);
           //INDIGO_DRIVER_LOG(DRIVER_NAME, "FOCUSER_ABORT_MOTION_PROPERTY");
           indigo_set_timer(device, 0, focuser_abort_callback, &PRIVATE_DATA->timer);
           FOCUSER_ABORT_MOTION_PROPERTY->state = INDIGO_OK_STATE;
           indigo_update_property(device, FOCUSER_ABORT_MOTION_PROPERTY, NULL);
           INDIGO_DRIVER_LOG(DRIVER_NAME, "Abort Motion handled");
           return INDIGO_OK;
    }
}

indigo_result indigo_focuser_adafruitmh(indigo_driver_action action, indigo_driver_info *info) {
        static indigo_device focuser_template = INDIGO_DEVICE_INITIALIZER(
                "Adafruit Motor Hat Focuser",
                focuser_attach,
                focuser_enumerate_properties,
                focuser_change_property,
                NULL,
                focuser_detach
            );


        switch (action) {
            case INDIGO_DRIVER_INIT:
                focuser_device = focuser_template;
                return indigo_attach_device(&focuser_device);
            case INDIGO_DRIVER_SHUTDOWN:
                return indigo_detach_device(&focuser_device);
            case INDIGO_DRIVER_INFO:
                if (info) {
                    memset(info, 0, sizeof(indigo_driver_info));
                    strncpy(info->name, "Adafruit Motor Hat Focuser", INDIGO_NAME_SIZE-1);
                    info->version = INDIGO_VERSION_CURRENT;
                }
                return INDIGO_OK;
            default: {
                return INDIGO_OK;
            }
        }
}
