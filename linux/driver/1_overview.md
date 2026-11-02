# Bus Protocols — Device

## Driver side

| Protocol | Device the driver receives | Driver struct | Register driver | `probe()` signature | Set / get drvdata |
| --- | --- | --- | --- | --- | --- |
| platform | `struct platform_device` | `struct platform_driver` | `module_platform_driver()` / `platform_driver_register()` | `probe(struct platform_device *)` | `platform_set_drvdata()` / `platform_get_drvdata()` |
| I2C | `struct i2c_client` | `struct i2c_driver` | `module_i2c_driver()` / `i2c_add_driver()` | `probe(struct i2c_client *)` | `i2c_set_clientdata()` / `i2c_get_clientdata()` |
| SPI | `struct spi_device` | `struct spi_driver` | `module_spi_driver()` / `spi_register_driver()` | `probe(struct spi_device *)` | `spi_set_drvdata()` / `spi_get_drvdata()` |
| USB | `struct usb_interface` | `struct usb_driver` | `module_usb_driver()` / `usb_register()` | `probe(struct usb_interface *, const struct usb_device_id *)` | `usb_set_intfdata()` / `usb_get_intfdata()` |
| HID | `struct hid_device` | `struct hid_driver` | `module_hid_driver()` / `hid_register_driver()` | `probe(struct hid_device *, const struct hid_device_id *)` | `hid_set_drvdata()` / `hid_get_drvdata()` |
| serdev (UART) | `struct serdev_device` | `struct serdev_device_driver` | `module_serdev_device_driver()` | `probe(struct serdev_device *)` | `serdev_device_set_drvdata()` / `serdev_device_get_drvdata()` |


