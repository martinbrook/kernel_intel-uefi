/*
 * HID Sensors Driver
 * Copyright (c) 2012, Intel Corporation.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
 *
 */
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/slab.h>
#include <linux/hid-sensor-hub.h>
#include <linux/iio/iio.h>
#include <linux/iio/trigger.h>
#include <linux/iio/sysfs.h>
#include "hid-sensor-trigger.h"

static int hid_sensor_conn_type = HID_USAGE_SENSOR_PROP_CONNEC_TYPE_PC_INT_ENUM;
module_param(hid_sensor_conn_type, int, 0644);
MODULE_PARM_DESC(hid_sensor_conn_type, "HID SENSOR CONNECT TYPE (1 to 3).");

static int hid_sensor_data_rdy_trigger_set_state(struct iio_trigger *trig,
						bool state)
{
	struct hid_sensor_common *st = iio_trigger_get_drvdata(trig);
	s32 power_state_val;
	s32 report_state_val;

	st->data_ready = state;
	if (state) {
		power_state_val = hid_sensor_get_usage_index(st->hsdev,
			st->power_state.report_id,
			st->power_state.index,
			HID_USAGE_SENSOR_PROP_POWER_STATE_D0_FULL_POWER_ENUM);
		report_state_val = hid_sensor_get_usage_index(st->hsdev,
			st->report_state.report_id,
			st->report_state.index,
			HID_USAGE_SENSOR_PROP_REPORTING_STATE_ALL_EVENTS_ENUM);
	} else {
		power_state_val = hid_sensor_get_usage_index(st->hsdev,
			st->power_state.report_id,
			st->power_state.index,
			HID_USAGE_SENSOR_PROP_POWER_STATE_D1_LOW_POWER_ENUM);
		report_state_val = hid_sensor_get_usage_index(st->hsdev,
			st->report_state.report_id,
			st->report_state.index,
			HID_USAGE_SENSOR_PROP_REPORTING_STATE_NO_EVENTS_ENUM);
	}
	power_state_val += st->power_state.logical_minimum;
	report_state_val += st->report_state.logical_minimum;
	if (power_state_val >= 0)
		sensor_hub_set_feature(st->hsdev, st->power_state.report_id,
						st->power_state.index,
						power_state_val);
	if (report_state_val >= 0)
		sensor_hub_set_feature(st->hsdev, st->report_state.report_id,
						st->report_state.index,
						report_state_val);

	/* Some hubs require this read as a 'sync' point. */
	sensor_hub_get_feature(st->hsdev, st->power_state.report_id,
					st->power_state.index,
					&power_state_val);

	sensor_hub_set_feature(st->hsdev, st->conn_type.report_id,
					st->conn_type.index,
					hid_sensor_conn_type);

	return 0;
}

void hid_sensor_remove_trigger(struct iio_dev *indio_dev)
{
	iio_trigger_unregister(indio_dev->trig);
	iio_trigger_free(indio_dev->trig);
	indio_dev->trig = NULL;
}
EXPORT_SYMBOL(hid_sensor_remove_trigger);

static const struct iio_trigger_ops hid_sensor_trigger_ops = {
	.owner = THIS_MODULE,
	.set_trigger_state = &hid_sensor_data_rdy_trigger_set_state,
};

int hid_sensor_setup_trigger(struct iio_dev *indio_dev, const char *name,
				struct hid_sensor_common *attrb)
{
	int ret;
	struct iio_trigger *trig;

	trig = iio_trigger_alloc("%s-dev%d", name, indio_dev->id);
	if (trig == NULL) {
		dev_err(&indio_dev->dev, "Trigger Allocate Failed\n");
		ret = -ENOMEM;
		goto error_ret;
	}

	trig->dev.parent = indio_dev->dev.parent;
	iio_trigger_set_drvdata(trig, attrb);
	trig->ops = &hid_sensor_trigger_ops;
	ret = iio_trigger_register(trig);

	if (ret) {
		dev_err(&indio_dev->dev, "Trigger Register Failed\n");
		goto error_free_trig;
	}
	indio_dev->trig = trig;

	return ret;

error_free_trig:
	iio_trigger_free(trig);
error_ret:
	return ret;
}
EXPORT_SYMBOL(hid_sensor_setup_trigger);

MODULE_AUTHOR("Srinivas Pandruvada <srinivas.pandruvada@intel.com>");
MODULE_DESCRIPTION("HID Sensor trigger processing");
MODULE_LICENSE("GPL");
