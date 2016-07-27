/**
 * @file drivers/linux/pin_port.i
 * @version 3.1.1
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

static int pin_port_module_init(void)
{
    return (0);
}

static int pin_port_init(struct pin_driver_t *self_p,
                         const struct pin_device_t *dev,
                         int mode)
{
    return (pin_set_mode(self_p, mode));
}

static int pin_port_read(struct pin_driver_t *self_p)
{
    return (0);
}

static int pin_port_write(struct pin_driver_t *self_p, int value)
{
    if (value != 0) {
        self_p->dev_p->regs_p->BSRR = (1 << self_p->dev_p->bit);
    } else {
        self_p->dev_p->regs_p->BSRR = (1 << (16 + self_p->dev_p->bit));
    }

    return (0);
}

static int pin_port_toggle(struct pin_driver_t *self_p)
{
    int value;

    value = ((self_p->dev_p->regs_p->ODR & (1 << self_p->dev_p->bit)) != 0);

    return (pin_port_write(self_p, !value));
}

static int pin_port_set_mode(struct pin_driver_t *self_p, int mode)
{
    int shift;
    volatile uint32_t *cr_p;
    struct pin_device_t  *dev_p;

    dev_p = self_p->dev_p;
    shift = (4 * dev_p->bit);
    cr_p = &dev_p->regs_p->CRL;

    if (dev_p->bit >= 8) {
        shift -= 32;
        cr_p++;
    }

    if (mode == PIN_OUTPUT) {
        *cr_p = bits_insert_32(*cr_p, shift, 4, 0x1);
    } else {
        *cr_p = bits_insert_32(*cr_p, shift, 4, 0x4);
    }

    return (0);
}
