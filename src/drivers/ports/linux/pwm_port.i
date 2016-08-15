/**
 * @file drivers/linux/pwm_port.i
 * @version 6.0.0
 *
 * @section License
 * Copyright (C) 2014-2016, Erik Moqvist
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * This file is part of the Simba project.
 */

static int pwm_port_init(struct pwm_driver_t *self_p,
                         const struct pwm_device_t *dev_p)
{
    return (0);
}

static int pwm_port_set_duty(struct pwm_driver_t *self_p,
                             uint8_t value)
{
    return (0);
}

static int pwm_port_get_duty(struct pwm_driver_t *self_p)
{
    return (-1);
}

static struct pwm_device_t *pwm_port_pin_to_device(struct pin_device_t *pin_p)
{
    return (NULL);
}
