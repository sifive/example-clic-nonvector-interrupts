/* Copyright 2020 SiFive, Inc */
/* SPDX-License-Identifier: Apache-2.0 */

#include <metal/cpu.h>
#include <metal/drivers/sifive_clic0.h>
#include <metal/led.h>
#include <metal/time.h>
#include <stdio.h>

#define RTC_FREQ	32768

#ifndef metal_led_ld0red
#define metal_led_ld0red metal_led_none
#endif

#ifndef metal_led_ld0green
#define metal_led_ld0green metal_led_none
#endif

#define led0_red metal_led_ld0red
#define led0_green metal_led_ld0green

struct metal_interrupt clic = (struct metal_interrupt){0};

void display_instruction (void) {
    printf("\n");
    printf("SIFIVE, INC.\n!!\n");
    printf("\n");
    printf("Coreplex IP Eval Kit 'clic-nonvector-interrupts' Example.\n\n");
    printf("IRQ 12 (CSIP) is enabled as an interrupt source.\n");
    printf("A 10s timer is used to trigger and clear CSIP interupt.\n");
    printf("\n");
}

void metal_riscv_cpu_intc_mtip_handler(void) {
    printf("**** Triggering CLIC Software Interrupt ****\n");
    sifive_clic0_set(clic, SIFIVE_CLIC_SOFTWARE_INTERRUPT_ID);
}

void metal_sifive_clic0_csip_handler(void) {
    struct metal_cpu cpu = metal_cpu_get(metal_cpu_get_current_hartid());
    printf("**** Caught CLIC Software Interrupt at time %u seconds ****\n", (uint32_t)metal_time());
    sifive_clic0_clear(clic, SIFIVE_CLIC_SOFTWARE_INTERRUPT_ID);
    printf("**** Toggling LEDs ****\n");
    metal_led_toggle(led0_green);
    metal_led_toggle(led0_red);
    printf("**** Re-arming timer for 10 seconds ****\n");
    metal_cpu_set_mtimecmp(cpu, metal_cpu_get_mtime(cpu) + 10*RTC_FREQ);
}

int main (void)
{
    metal_led_enable(led0_red);
    metal_led_enable(led0_green);
    metal_led_on(led0_red);
    metal_led_off(led0_green);
 
    display_instruction();

    sifive_clic0_enable(clic, SIFIVE_CLIC_SOFTWARE_INTERRUPT_ID);

    struct metal_cpu cpu = metal_cpu_get(metal_cpu_get_current_hartid());
    metal_cpu_set_mtimecmp(cpu, metal_cpu_get_mtime(cpu) + 10*RTC_FREQ);
    metal_cpu_enable_timer_interrupt();
    metal_cpu_enable_interrupts();

    while (1) {
        __asm__ volatile ("wfi");
    }

    return 0;
}
