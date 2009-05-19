/*
 * drivers/usb/host/ohci-mipc.c
 *
 * Nintendo Wii USB Open Host Controller Interface via 'mini' IPC (mipc).
 * Copyright (C) 2009 The GameCube Linux Team
 * Copyright (C) 2009 Albert Herranz
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at
 * your option) any later version.
 *
 * Based on ohci-ppc-of.c
 *
 * OHCI HCD (Host Controller Driver) for USB.
 *
 * (C) Copyright 1999 Roman Weissgaerber <weissg@vienna.at>
 * (C) Copyright 2000-2002 David Brownell <dbrownell@users.sourceforge.net>
 * (C) Copyright 2002 Hewlett-Packard Company
 * (C) Copyright 2006 Sylvain Munaut <tnt@246tNt.com>
 *
 * Bus glue for OHCI HC on the of_platform bus
 *
 * Modified for of_platform bus from ohci-sa1111.c
 *
 * This file is licenced under the GPL.
 */

#include <linux/signal.h>
#include <linux/of_platform.h>

#include <asm/prom.h>
#include <asm/starlet.h>

#define DRV_MODULE_NAME "ohci-mipc"
#define DRV_DESCRIPTION "USB Open Host Controller Interface for MINI"
#define DRV_AUTHOR      "Albert Herranz"

#define HOLLYWOOD_EHCI_CTL 0x0d0400cc
#define HOLLYWOOD_EHCI_CTL_OH0INTE	(1<<11)
#define HOLLYWOOD_EHCI_CTL_OH1INTE	(1<<12)

static int __devinit
ohci_mipc_start(struct usb_hcd *hcd)
{
	struct ohci_hcd	*ohci = hcd_to_ohci(hcd);
	void *ehci_ctl = (void *)HOLLYWOOD_EHCI_CTL;
	int error;

	error = ohci_init(ohci);
	if (error)
		goto out;

	/* enable notification of OHCI interrupts */
	mipc_out_be32(ehci_ctl, mipc_in_be32(ehci_ctl) | 0xe0000 |
						HOLLYWOOD_EHCI_CTL_OH0INTE |
						HOLLYWOOD_EHCI_CTL_OH1INTE);

	error = ohci_run(ohci);
	if (error) {
		err("can't start %s", ohci_to_hcd(ohci)->self.bus_name);
		ohci_stop(hcd);
		goto out;
	}

out:
	return error;
}

static const struct hc_driver ohci_mipc_hc_driver = {
	.description =		hcd_name,
	.product_desc =		"Nintendo Wii OHCI Host Controller",
	.hcd_priv_size =	sizeof(struct ohci_hcd),

	/*
	 * generic hardware linkage
	 */
	.irq =			ohci_irq,
	.flags =		HCD_USB11 | HCD_BOUNCE_DMA_MEM,

	/*
	 * basic lifecycle operations
	 */
	.start =		ohci_mipc_start,
	.stop =			ohci_stop,
	.shutdown = 		ohci_shutdown,

	/*
	 * managing i/o requests and associated device resources
	 */
	.urb_enqueue =		ohci_urb_enqueue,
	.urb_dequeue =		ohci_urb_dequeue,
	.endpoint_disable =	ohci_endpoint_disable,

	/*
	 * scheduling support
	 */
	.get_frame_number =	ohci_get_frame,

	/*
	 * root hub support
	 */
	.hub_status_data =	ohci_hub_status_data,
	.hub_control =		ohci_hub_control,
#ifdef	CONFIG_PM
	.bus_suspend =		ohci_bus_suspend,
	.bus_resume =		ohci_bus_resume,
#endif
	.start_port_reset =	ohci_start_port_reset,
};


static int __devinit
ohci_hcd_mipc_probe(struct of_device *op, const struct of_device_id *match)
{
	struct device_node *dn = op->node;
	struct usb_hcd *hcd;
	struct ohci_hcd	*ohci = NULL;
	struct resource res;
	int irq;
	int error;

	if (usb_disabled())
		return -ENODEV;

	if (starlet_get_ipc_flavour() != STARLET_IPC_MINI)
		return -ENODEV;

	dev_dbg(&op->dev, "initializing " DRV_MODULE_NAME " USB Controller\n");

	error = of_address_to_resource(dn, 0, &res);
	if (error)
		goto out;

	hcd = usb_create_hcd(&ohci_mipc_hc_driver, &op->dev, DRV_MODULE_NAME);
	if (!hcd) {
		error = -ENOMEM;
		goto out;
	}

	hcd->rsrc_start = res.start;
	hcd->rsrc_len = res.end - res.start + 1;

	irq = irq_of_parse_and_map(dn, 0);
	if (irq == NO_IRQ) {
		printk(KERN_ERR __FILE__ ": irq_of_parse_and_map failed\n");
		error = -EBUSY;
		goto err_irq;
	}

	hcd->regs = (void __iomem *)(unsigned long)hcd->rsrc_start;

	ohci = hcd_to_ohci(hcd);
	ohci_hcd_init(ohci);

	error = usb_add_hcd(hcd, irq, IRQF_DISABLED);
	if (error)
		goto err_add_hcd;

	return 0;

err_add_hcd:
	irq_dispose_mapping(irq);
err_irq:
	usb_put_hcd(hcd);
out:
	return error;
}

static int ohci_hcd_mipc_remove(struct of_device *op)
{
	struct usb_hcd *hcd = dev_get_drvdata(&op->dev);

	dev_set_drvdata(&op->dev, NULL);

	dev_dbg(&op->dev, "stopping " DRV_MODULE_NAME " USB Controller\n");

	usb_remove_hcd(hcd);
	irq_dispose_mapping(hcd->irq);
	usb_put_hcd(hcd);

	return 0;
}

static int ohci_hcd_mipc_shutdown(struct of_device *op)
{
	struct usb_hcd *hcd = dev_get_drvdata(&op->dev);

	if (hcd->driver->shutdown)
		hcd->driver->shutdown(hcd);

	return 0;
}


static struct of_device_id ohci_hcd_mipc_match[] = {
	{
		.compatible = "nintendo,hollywood-ohci",
	},
	{},
};
MODULE_DEVICE_TABLE(of, ohci_hcd_mipc_match);

static struct of_platform_driver ohci_hcd_mipc_driver = {
	.name		= DRV_MODULE_NAME,
	.match_table	= ohci_hcd_mipc_match,
	.probe		= ohci_hcd_mipc_probe,
	.remove		= ohci_hcd_mipc_remove,
	.shutdown 	= ohci_hcd_mipc_shutdown,
	.driver		= {
		.name	= DRV_MODULE_NAME,
		.owner	= THIS_MODULE,
	},
};

