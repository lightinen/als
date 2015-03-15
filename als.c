#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/acpi.h>

MODULE_AUTHOR("Viktar Vauchkevich <victorenator@gmail.com>");
MODULE_DESCRIPTION("Ambient Light Sensor Driver");
MODULE_LICENSE("GPL");
/*
 *  ALS enable
 */
static int alae = 0;
module_param(alae, uint, 0444);
MODULE_PARM_DESC(alae, "ALAE value (*0 disabled, 1 enabled)");

ACPI_MODULE_NAME("ALS")

static int als_add(struct acpi_device *device);
static int als_remove(struct acpi_device *device);
static void als_notify(struct acpi_device *device, u32 event);

static const struct acpi_device_id als_device_ids[] = {
	{"ACPI0008", 0},
	{},
};
MODULE_DEVICE_TABLE(acpi, als_device_ids);

static struct acpi_driver als_driver = {
	.name = "als",
	.class = "ALS",
	.ids = als_device_ids,
	.ops =
	{
		.add = als_add,
		.remove = als_remove,
		.notify = als_notify
	},
	.owner = THIS_MODULE,
};


static u32 als_set_int(acpi_handle handle, const char *func, int param)
{
	struct acpi_object_list params;
	union acpi_object in_obj;
	u64 ret;
	acpi_status status;
	params.count = 1;
	params.pointer = &in_obj;
	in_obj.type = ACPI_TYPE_INTEGER;
	in_obj.integer.value = param;

	status = acpi_evaluate_integer(handle, (acpi_string) func, &params, &ret);
	if(!ACPI_SUCCESS(status))
	    ACPI_ERROR((AE_INFO,"Integer evaluation for %s failed[%d]",func,status));

	return ret;
}
static u32 als_get_int(acpi_handle handle, const char *func)
{
	u64 val = 1844;
	acpi_status status;

	status = acpi_evaluate_integer(handle, (acpi_string) func, NULL, &val);

	if(!ACPI_SUCCESS(status))
	    ACPI_ERROR((AE_INFO,"Integer evaluation for %s failed[%d]",func,status));

	return val;
}
static ssize_t als_show_als(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct acpi_device *device = to_acpi_device(dev);

	return sprintf(buf,
			"Local(ALSC/ALAE):          %d\n"
			"\\_SB_.ALAE:                %d\n"
			"\\_SB_.LSTP:                %d\n"
			"\\_SB_.BRTI:                %d\n"
			"\\_SB_.ALS_._ALI:           %d\n"
			"\\_SB_.ATKD.GALS:           %d\n"
			"\\_SB_.PCI0.LPCB.EC0_.RALS: %d\n",
			alae, als_get_int(NULL,"\\_SB_.ALAE"),
			als_get_int(NULL,"\\_SB_.LSTP"),
			als_get_int(NULL,"\\_SB_.BRTI"),
			als_get_int(device->handle,"_ALI"), 
			als_get_int(NULL,"\\_SB.ATKD.GALS"), 
			als_get_int(NULL,"\\_SB_.PCI0.LPCB.EC0_.RALS"));
}
static ssize_t als_write_alsc(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t len)
{
	int ret;
	uint num;
	//struct acpi_device *device = to_acpi_device(dev);

	if (!buf)
		return -EINVAL;
	ret = kstrtouint(buf, 0, &num);
	if (ret == -EINVAL || num < 0 || num > 1)
		return -EINVAL;
	alae = num;
	return als_set_int(NULL,"\\_SB_.ATKD.ALSC",num);
}
static DEVICE_ATTR(als, S_IRUGO, als_show_als, NULL);
static DEVICE_ATTR(alsc, S_IWUSR, NULL, als_write_alsc);

static struct attribute *als_attributes[] = {
	&dev_attr_als.attr,
	&dev_attr_alsc.attr,
	NULL
};

static const struct attribute_group als_attr_group = {
	.attrs = als_attributes,
};

static int als_add(struct acpi_device *device)
{
	int result = als_set_int(NULL,"\\_SB_.ATKD.ALSC",alae);
	result = sysfs_create_group(&device->dev.kobj, &als_attr_group);
	return result;
}
	
static int als_remove(struct acpi_device *device)
{
	sysfs_remove_group(&device->dev.kobj, &als_attr_group);
	return 0;
}

static void als_notify(struct acpi_device* device, u32 event)
{
	pr_info("als_notify %x %x\n", event, als_get_int(device,"_ALI"));
	kobject_uevent(&device->dev.kobj, KOBJ_CHANGE);
}

static int __init als_init(void)
{
	int result = 0;

	result = acpi_bus_register_driver(&als_driver);
	if (result < 0) {
		ACPI_DEBUG_PRINT((ACPI_DB_ERROR,
			"Error registering driver\n"));
		return -ENODEV;
	}

	return 0;
}

static void __exit als_exit(void)
{
	acpi_bus_unregister_driver(&als_driver);
}


module_init(als_init);
module_exit(als_exit);
