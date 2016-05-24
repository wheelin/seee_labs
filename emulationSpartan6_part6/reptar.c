#include "hw/arm/arm.h"
#include "qemu-common.h"
#include "sysemu/sysemu.h"
#include "hw/arm/omap.h"
#include "hw/boards.h"
#include "hw/i2c/i2c.h"
#include "net/net.h"
#include "hw/devices.h"
#include "hw/block/flash.h"
#include "hw/sysbus.h"
#include "sysemu/blockdev.h"
#include "exec/address-spaces.h"
#include "hw/qdev.h"

/* (DRE) */
static const int sector_len = 4 * 1024;

#define REPTAR_NAND_CS       0
#define REPTAR_NET_CS        5

struct reptar_s {
    struct omap_mpu_state_s *cpu;

    DeviceState *nand;
    void *twl4030;
    DeviceState *eth;
    DeviceState *ddc;
    DeviceState *sp6;
};

struct arm_boot_info reptar_binfo;

static void reptar_init(MachineState *machine)
{
    MemoryRegion *sysmem = get_system_memory();
    struct reptar_s *s = (struct reptar_s *)g_malloc0(sizeof(*s));
    DriveInfo *dmtd = NULL, *dsd = NULL, *pflash = NULL;
    const char *kernel_filename = machine->kernel_filename;
    const char *kernel_cmdline = machine->kernel_cmdline;
    const char *initrd_filename = machine->initrd_filename;

    /* (DRE) */
    uint32_t pflash_size = 16 << 10; /* 16 KB */

    if (ram_size > 1024 * 1024 * 1024) {
        fprintf(stderr, "reptar: maximum permitted RAM size 1024MB\n");
        exit(1);
    }

    /* Check if we are in -kernel u-boot.bin mode */
    if (kernel_filename) {
        /* (DRE) We do the following in order to retrieve the kernel filename during the boot process.
         * For example, if qemu was launched with -kernel u-boot.bin */
        reptar_binfo.ram_size = ram_size;
        reptar_binfo.kernel_filename = kernel_filename;
        reptar_binfo.kernel_cmdline = kernel_cmdline;
        reptar_binfo.initrd_filename = initrd_filename;
        reptar_binfo.loader_start = 0x40014100; /* Start of small bootloader*/
        reptar_binfo.board_id = 0;
    } else
        reptar_binfo.kernel_filename = NULL;

    s->cpu = omap3_mpu_init(sysmem, omap3430, ram_size, NULL, NULL, serial_hds[0], NULL);

    pflash = drive_get(IF_PFLASH, 0, 0);
    if (pflash) {
        if (!pflash_cfi02_register(0x30000000,
                                   NULL,
                                   "reptar.nand_flash",
                                   pflash_size,
                                   blk_by_legacy_dinfo(pflash),
                                   sector_len,
                                   pflash_size / sector_len,
                                   1, 2, 0, 0, 0, 0, 0x555, 0x2AA, 0)) {
            fprintf(stderr, "qemu: Error registering flash memory.\n");
            exit(1);
        }
    }
    else {

        dmtd = drive_get(IF_MTD, 0, 0);

        s->nand = nand_init(dmtd ? blk_by_legacy_dinfo(dmtd) : NULL, NAND_MFR_MICRON, 0xba);

        nand_setpins(s->nand, 0, 0, 0, 1, 0); /* no write-protect */
        omap_gpmc_attach_nand(s->cpu->gpmc, REPTAR_NAND_CS, s->nand);

    }

    /*
     * (DRE) Define MMC if any
     */
    dsd  = drive_get(IF_SD, 0, 0);
    if (dsd)
        omap3_mmc_attach(s->cpu->omap3_mmc[0], blk_by_legacy_dinfo(dsd), 0, 0);


    if (!pflash && !dmtd && !dsd)
        hw_error("%s: SD or NAND image required", __FUNCTION__);

    /* FAB revs >= 2516: 4030 interrupt is GPIO 0 (earlier ones were 112) */
    s->twl4030 = twl4030_init(omap_i2c_bus(s->cpu->i2c[0]),
                              qdev_get_gpio_in(s->cpu->gpio, 0),
                              NULL,
                              NULL);

    /* Wire up an I2C slave which returns EDID monitor information;
     * newer Linux kernels won't turn on the display unless they
     * detect a monitor over DDC.
     */
    s->ddc = i2c_create_slave(omap_i2c_bus(s->cpu->i2c[2]), "i2c-ddc", 0x50);

    omap_lcd_panel_attach(s->cpu->dss);

    /* Strictly this should be a LAN9221 */
#if 0
    if (nd_table[0].vlan) {
#endif /* 0 */
        /* The ethernet chip hangs off the GPMC */
        NICInfo *nd = &nd_table[0];
        qemu_check_nic_model(nd, "lan9118");
        s->eth = qdev_create(NULL, "lan9118");
        qdev_set_nic_properties(s->eth, nd);
        qdev_init_nofail(s->eth);
        omap_gpmc_attach(s->cpu->gpmc, REPTAR_NET_CS,
                         sysbus_mmio_get_region(SYS_BUS_DEVICE(s->eth), 0));
        sysbus_connect_irq(SYS_BUS_DEVICE(s->eth), 0, qdev_get_gpio_in(s->cpu->gpio, 29));
#if 0
    }
#endif /* 0 */

    // Create reptar_sp6 with it's base address
	s->sp6 = sysbus_create_simple("reptar_sp6",0x18000000,NULL);
	
	// Connect reptar_sp6 to GPIO10 for the IRQ
	sysbus_connect_irq(SYS_BUS_DEVICE(s->sp6), 0, qdev_get_gpio_in(s->cpu->gpio, 10));
}

static QEMUMachine reptar_machine = {
    .name =        "reptar",
    .desc =        "REPTAR board (based on OMAP3530)",
    .init =        reptar_init,
};

static void reptar_machine_init(void)
{
    qemu_register_machine(&reptar_machine);
}

machine_init(reptar_machine_init);
