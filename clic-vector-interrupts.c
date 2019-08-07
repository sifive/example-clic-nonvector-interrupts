/* Copyright 2019 SiFive, Inc */
/* SPDX-License-Identifier: Apache-2.0 */

#include <stdio.h>
#include <string.h>
#include <metal/cpu.h>
#include <metal/led.h>
#include <metal/button.h>
#include <metal/switch.h>

#define RTC_FREQ	32768

struct metal_cpu *cpu;
struct metal_interrupt *cpu_intr, *tmr_intr;
struct metal_led *led0_red, *led0_green, *led0_blue;
struct metal_interrupt *clic;
int tmr_id, swch1_irq, swch2_irq;

void display_instruction (void) {
    printf("\n");
    printf("SIFIVE, INC.\n!!\n");
    printf("\n");
    printf("Coreplex IP Eval Kit 'clic-vector-interrupts' Example.\n\n");
    printf("IRQ 47 and 48 are enabled as extrenal interrupt sources.\n");
    printf("A 10s debounce timer is used between these interupts.\n");
    printf("\n");
}

void timer_isr (int id, void *data) {
    static int sw1_win = 1; 

    if (sw1_win) {
        printf("#### Giving Switch 2 Priority for 10 seconds ####\n");
        // Main setup both switches with priority 2, but lower irq will have
        // higher priority and win. So give sw2 highier priority.
        metal_interrupt_set_priority(clic, swch1_irq, 3);
        metal_interrupt_set(clic, swch1_irq);
    } else {
        printf("**** Giving Switch 1 Priority for 10 seconds ****\n");
        // Lower sw2 priority back, so sw1 have a chance.
        metal_interrupt_set_priority(clic, swch1_irq, 2);
        metal_interrupt_set(clic, swch1_irq);
    }    
    sw1_win ^= 0x1;

    metal_cpu_set_mtimecmp(cpu, metal_cpu_get_mtime(cpu) + 10*RTC_FREQ);
}

void switch1_isr(int id, void *data) {
    metal_interrupt_clear(clic, swch1_irq);
    printf("Switch 1 is on via IRQ %d!\n", id);
    printf("Even if Switch 2 is on, Switch 1 have higher priority right now.\n");
    metal_led_on(led0_green);
    metal_led_off(led0_red);
}

void switch2_isr(int id, void *data) {
    metal_interrupt_clear(clic, swch2_irq);
    printf("Switch 2 is on via IRQ %d!\n", id);
    printf("Even if Switch 1 is on, Switch 2 have higher priority right now.\n");
    metal_led_off(led0_green);
    metal_led_on(led0_red);
}

int main (void)
{
    int rc;

    // Lets get start with getting LEDs and turn only RED ON
    led0_red = metal_led_get_rgb("LD0", "red");
    led0_green = metal_led_get_rgb("LD0", "green");
    led0_blue = metal_led_get_rgb("LD0", "blue");
    if ((led0_red == NULL) || (led0_green == NULL) || (led0_blue == NULL)) {
        printf("Abort. At least one of LEDs is null.\n");
        return 1;
    }
    metal_led_enable(led0_red);
    metal_led_enable(led0_green);
    metal_led_enable(led0_blue);
    metal_led_on(led0_red);
    metal_led_off(led0_green);
    metal_led_off(led0_blue);
 
    // Lets get the CPU and and its interrupt
    cpu = metal_cpu_get(metal_cpu_get_current_hartid());
    if (cpu == NULL) {
        printf("Abort. CPU is null.\n");
        return 2;
    }
    cpu_intr = metal_cpu_interrupt_controller(cpu);
    if (cpu_intr == NULL) {
        printf("Abort. CPU interrupt controller is null.\n");
        return 3;
    }
    metal_interrupt_init(cpu_intr);

    // Setup Timer interrupt
    tmr_intr = metal_cpu_timer_interrupt_controller(cpu);
    if (tmr_intr == NULL) {
        printf("Abort. TIMER interrupt controller is  null.\n");
        return 4;
    }
    metal_interrupt_init(tmr_intr);
    tmr_id = metal_cpu_timer_get_interrupt_id(cpu);
    rc = metal_interrupt_register_handler(tmr_intr, tmr_id, timer_isr, cpu);
    if (rc < 0) {
        printf("Failed. TIMER interrupt handler registration failed\n");
        return (rc * -1);
    }

    clic = metal_interrupt_get_controller(METAL_CLIC_CONTROLLER,
                                          metal_cpu_get_current_hartid());
    if (clic == NULL) {
        printf("Exit. This example need a clic interrupt controller.\n");
        return 0;
    }
/*
MIE disable
Walk through the IRQs and set the following;
Then with irq-id, you can now change enable/disable, vector-enable/disable, level/priority, mode?
MIE enable
*/
    metal_interrupt_init(clic);
    metal_interrupt_set_vector_mode(clic, METAL_SELECTIVE_NONVECTOR_MODE);
    //metal_interrupt_privilege_mode(clic, METAL_INTR_PRIV_M_MODE);
    //metal_interrupt_threshold_level(clic, 2);

    //swch1_irq = metal_switch_get_interrupt_id(swch1);
    // swch1 = irq-47 on e21-arty
    swch1_irq = 12;
    //swch1_irq = metal_switch_get_interrupt_id(swch1);
    rc = metal_interrupt_register_handler(clic, swch1_irq, switch1_isr, NULL);
    if (rc < 0) {
        printf("SW1 interrupt handler registration failed\n");
        return (rc * -1);
    }
    //swch2_irq = 48;
    //rc = metal_interrupt_register_handler(clic, swch2_irq, switch2_isr, NULL);
    //if (rc < 0) {
    //    printf("SW2 interrupt handler registration failed\n");
    //    return (rc * -1);
    //}

    // Lets enable Switches interrupts
    metal_interrupt_set_threshold(clic, 1);
    metal_interrupt_set_priority(clic, swch1_irq, 2);
    //metal_interrupt_set_priority(clic, swch2_irq, 2);
    if (metal_interrupt_enable(clic, swch1_irq) == -1) {
        printf("SW1 interrupt enable failed\n");
        return 5;
    }
    //if (metal_interrupt_enable(clic, swch2_irq) == -1) {
    //    printf("SW2 interrupt enable failed\n");
    //    return 5;
    //}
    // Set timeout of 10s, and enable timer interrupt
    metal_cpu_set_mtimecmp(cpu, metal_cpu_get_mtime(cpu) + 10*RTC_FREQ);
    metal_interrupt_enable(tmr_intr, tmr_id);

    // Lastly CPU interrupt
    if (metal_interrupt_enable(cpu_intr, 0) == -1) {
        printf("CPU interrupt enable failed\n");
        return 6;
    }

    display_instruction();

    while (1) {
        asm volatile ("wfi");
    }

    return 0;
}
