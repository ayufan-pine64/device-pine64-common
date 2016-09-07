#define LOG_TAG "test"

#include <hardware/hdmi_cec.h>

#include <stdlib.h>
#include <pthread.h>
#include <stdio.h>
#include <android/log.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "log.h"

extern struct hw_module_t HAL_MODULE_INFO_SYM;

#define ME 8
#define BRD CEC_ADDR_BROADCAST

static int send_cec_message(hdmi_cec_device_t *dev, int initiator, int destination, const unsigned char *data,
                            size_t length) {
    cec_message_t msg;
    msg.initiator = initiator;
    msg.destination = destination;
    msg.length = length;
    memcpy(msg.body, data, length);
    return dev->send_message(dev, &msg);
}

static void send_report_physical_address(hdmi_cec_device_t *dev, int destination)
{
	uint16_t address = 0;
    dev->get_physical_address(dev, &address);
    unsigned char data[] = {CEC_MESSAGE_REPORT_PHYSICAL_ADDRESS, address >> 8, address, CEC_DEVICE_PLAYBACK};
    send_cec_message(dev, ME, BRD, data, 4);
}

static void send_device_vendor_id(hdmi_cec_device_t *dev, int destination)
{
	uint16_t address = 0;
    dev->get_physical_address(dev, &address);
    unsigned char data[] = {CEC_MESSAGE_DEVICE_VENDOR_ID, 0x0, 0x15, 0x82};
    send_cec_message(dev, ME, BRD, data, sizeof(data));
}

static void send_active_source(hdmi_cec_device_t *dev, int destination)
{
	uint16_t address = 0;
    dev->get_physical_address(dev, &address);
    unsigned char data[] = {CEC_MESSAGE_ACTIVE_SOURCE, address >> 8, address};
    send_cec_message(dev, ME, destination, data, sizeof(data));
}

static void send_osd_name(hdmi_cec_device_t *dev, int destination)
{
    unsigned char data[] = {CEC_MESSAGE_SET_OSD_NAME, 'A', 'B', 'C'};
    send_cec_message(dev, ME, destination, data, sizeof(data));
}

static void send_cec_version(hdmi_cec_device_t *dev, int destination)
{
    unsigned char data[] = {CEC_MESSAGE_CEC_VERSION, 0x05};
    send_cec_message(dev, ME, destination, data, sizeof(data));
}

static void send_device_power_status(hdmi_cec_device_t *dev, int destination, int status)
{
    unsigned char data[] = {CEC_MESSAGE_REPORT_POWER_STATUS, status};
    send_cec_message(dev, ME, destination, data, sizeof(data));
}

static void
cec_event(hdmi_cec_device_t *dev, int initiator, int destination, const unsigned char *data, size_t length) {
    switch (data[0]) {
    case CEC_MESSAGE_ACTIVE_SOURCE:
    	send_device_vendor_id(dev, BRD);
    	break;

    case CEC_MESSAGE_REQUEST_ACTIVE_SOURCE:
    	dev->set_option(dev, HDMI_OPTION_SYSTEM_CEC_CONTROL, 1);
	    send_active_source(dev, initiator);
    	break;

    case CEC_MESSAGE_GIVE_DEVICE_VENDOR_ID:
    	send_device_vendor_id(dev, initiator);
    	break;

    case CEC_MESSAGE_GIVE_OSD_NAME:
	    send_osd_name(dev, initiator);
    	break;

    case CEC_MESSAGE_STANDBY:
    	dev->set_option(dev, HDMI_OPTION_SYSTEM_CEC_CONTROL, 0);
    	break;

    case CEC_MESSAGE_GET_CEC_VERSION:
    	send_cec_version(dev, initiator);
    	break;

    case CEC_MESSAGE_GIVE_DEVICE_POWER_STATUS:
	    send_device_power_status(dev, initiator, 0);
    	break;

    case CEC_MESSAGE_GIVE_PHYSICAL_ADDRESS:
    	send_report_physical_address(dev, initiator);
    	break;
    }
}

static void broadcast_me(hdmi_cec_device_t* device)
{
    send_report_physical_address(device, BRD);
    send_device_vendor_id(device, BRD);
}

static void callback(const hdmi_event_t* event, hdmi_cec_device_t* dev)
{
	ALOGI("callback received: %d", event->type);

	switch(event->type) {
	case HDMI_EVENT_CEC_MESSAGE:
        if (event->cec.length >= 1) {
            cec_event(dev, event->cec.initiator, event->cec.destination,
            	event->cec.body, event->cec.length);
        }
		break;

	case HDMI_EVENT_HOT_PLUG:
		if (event->hotplug.connected) {
			broadcast_me(dev);
		}
		break;
	}
}

int main(int argc, char *argv[])
{
    int err;
    hw_module_t* module = &HAL_MODULE_INFO_SYM;

    hdmi_cec_device_t* device = NULL;
    err = module->methods->open(module, HDMI_CEC_HARDWARE_INTERFACE, (hw_device_t **)&device);
    if (err != 0) {
        ALOGE("Error opening hardware module: %d", err);
        return 1;
    }

    ALOGI("Starting...");
    device->clear_logical_address(device);
    device->add_logical_address(device, ME);
    device->set_option(device, HDMI_OPTION_WAKEUP, 1);
    //device->set_option(device, HDMI_OPTION_ENABLE_CEC, 0);
    device->set_option(device, HDMI_OPTION_ENABLE_CEC, 1);
    //device->set_option(device, HDMI_OPTION_SYSTEM_CEC_CONTROL, 0);
    device->set_option(device, HDMI_OPTION_SYSTEM_CEC_CONTROL, 1);

    device->register_event_callback(device, (event_callback_t)callback, device);

	broadcast_me(device);
    //send_active_source(device, BRD);
    //send_osd_name(device, BRD);

    ALOGI("initialised");
    sleep(3600);
    return 0;
}
