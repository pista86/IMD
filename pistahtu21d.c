// SPDX-License-Identifier: GPL-2.0
#include <linux/init.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/i2c.h>
#include <linux/utsname.h>
#include <linux/string.h>
#include <linux/types.h>
#include <linux/delay.h>

/* Add your code here */

#define  DEVICE_NAME "i2c-pistahtu21d"    ///< The device will appear at /dev/i2c using this value
#define  CLASS_NAME  "i2c"                ///< The device class -- this is a character device driver

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("IMD 2019 Marcelo Pistarelli");
MODULE_AUTHOR("Marcelo Pistarelli");
MODULE_VERSION("0.1");

static int    majorNumber;                  ///< Stores the device number -- determined automatically
static int    numberOpens = 0;              ///< Counts the number of times the device is opened
static struct class*  i2cClass  = NULL;     ///< The device-driver class struct pointer
static struct device* i2cDevice = NULL;     ///< The device-driver device struct pointer
static struct i2c_client *puertoi2c;
 
/** @brief Devices are represented as file structure in the kernel. The file_operations structure from
 *  /linux/fs.h lists the callback functions that you wish to associated with your file operations
 *  using a C99 syntax structure. char devices usually implement open, read, write and release calls
 */


// The prototype functions for the character driver -- must come before the struct definition
static int     dev_open(struct inode *, struct file *);
static int     dev_release(struct inode *, struct file *);
static ssize_t dev_read(struct file *, char *, size_t, loff_t *);
static ssize_t dev_write(struct file *, const char *, size_t, loff_t *);

static struct file_operations fops =
{
   .open = dev_open,
   .read = dev_read,
   .write = dev_write,
   .release = dev_release,
};

typedef struct {
   uint8_t entero;
   uint8_t decimal;
   uint8_t lecturaValida;
} medicion_result_t;

static int htu21_crc_check( uint16_t value, uint8_t crc);
 
// De hoja de datos
static const char TRIGGER_HUMIDITY_MEASUREMENT = 0xF5; // no hold master
static const char TRIGGER_TEMPERATURE_MEASUREMENT = 0xF3; // no hold master
static const char WRITE_USER_REGISTER = 0xE6; 
static const char READ_USER_REGISTER = 0xE7;

static const struct i2c_device_id pistahtu21d_id[] = {
    { "pistahtu21d", 0 },
    { }
};

MODULE_DEVICE_TABLE(i2c, pistahtu21d_id);

static const struct of_device_id pistahtu21d_of_match[] = {
    { .compatible = "mse,pistahtu21d" },
    { }
};

MODULE_DEVICE_TABLE(of, pistahtu21d_of_match);


static int ebbchar_init(void){
   pr_info("i2c: Inicializando el driver i2c\n");
 
   // Try to dynamically allocate a major number for the device -- more difficult but worth it
   majorNumber = register_chrdev(0, DEVICE_NAME, &fops);
   if (majorNumber<0){
      pr_alert("No se pudo asignar un numero mayor a i2c\n");
      return majorNumber;
   }
   pr_info("i2c registrado exitosamente con numero mayor  %d\n", majorNumber);
 
   // Register the device class
   i2cClass = class_create(THIS_MODULE, CLASS_NAME);
   if (IS_ERR(i2cClass)){                // Check for error and clean up if there is
      unregister_chrdev(majorNumber, DEVICE_NAME);
      pr_alert("No fue posible registrar la clase\n");
      return PTR_ERR(i2cClass);          // Correct way to return an error on a pointer
   }
   pr_info("i2c: clase registrada correctamente\n");
 
   // Register the device driver
   i2cDevice = device_create(i2cClass, NULL, MKDEV(majorNumber, 0), NULL, DEVICE_NAME);
   if (IS_ERR(i2cDevice)){               // Clean up if there is an error
      class_destroy(i2cClass);           // Repeated code but the alternative is goto statements
      unregister_chrdev(majorNumber, DEVICE_NAME);
      pr_alert("Error al crear el dispositivo\n");
      return PTR_ERR(i2cDevice);
   }
   pr_info("i2c: clase de dispositivo creada correctamente\n"); // Made it! device was initialized
   return 0;
}

static void ebbchar_exit(void){
   device_destroy(i2cClass, MKDEV(majorNumber, 0));     // remove the device
   class_unregister(i2cClass);                          // unregister the device class
   class_destroy(i2cClass);                             // remove the device class
   unregister_chrdev(majorNumber, DEVICE_NAME);             // unregister the major number
   pr_info("i2c: remocion exitosa.\n");
}

static int dev_open(struct inode *inodep, struct file *filep){
   numberOpens++;
   pr_info("i2c: Se ha abierto el dispositivo %d vece(s)\n", numberOpens);
   return 0;
}

static ssize_t dev_read(struct file *filep, char *buffer, size_t len, loff_t *offset){
    int error_count = 0;
    char data[3];
    uint16_t lecturaHumedad;
    uint16_t lecturaTemperatura;

    // Se inicia la medición de humedad espero según hoja de datos a que termine
    if (i2c_master_send(puertoi2c, &TRIGGER_HUMIDITY_MEASUREMENT, 1) > 0)
    {
        msleep(20);
        // Lectura de humedad
        if(i2c_master_recv(puertoi2c, data, 3)>0)
        {
            lecturaHumedad = (data[0] << 8) + data[1];
            if (htu21_crc_check( lecturaHumedad, data[2] ) == 0)
            {  
                // Lectura de humedad correcta
                error_count += copy_to_user(&buffer[0], &lecturaHumedad, 2);
            }
        } 
    }

    // Se inicia la medición de temperatura espero según hoja de datos a que termine
    if (i2c_master_send(puertoi2c, &TRIGGER_TEMPERATURE_MEASUREMENT, 1) > 0)
    {
        msleep(50);
        // Lectura de temperatura
        if(i2c_master_recv(puertoi2c, data, 3)>0)
        {
            lecturaTemperatura = (data[0] << 8) + data[1];
            if (htu21_crc_check( lecturaTemperatura, data[2] ) == 0)
            {  
                // Lectura de temperatura correcta
                error_count += copy_to_user(&buffer[2], &lecturaTemperatura, 2);
            }
        } 
    }

   if (error_count==0){            // if true then have success
      //pr_info("i2c: Se enviaron %d caracteres al usuario\n", Ret);
      return 0;
   }
   else {
      pr_info("EBBChar: Failed to send %d characters to the user\n", error_count);
      return -EFAULT;              // Failed -- return a bad address message (i.e. -14)
   }
}
 

static ssize_t dev_write(struct file *filep, const char *buffer, size_t len, loff_t *offset){
    char resolucionTemperatura;
    char sensorConfig;
    char sensorConfigVerif;
    char configToWrite[2];

    copy_from_user(&resolucionTemperatura, buffer, 1);

    // Enviar comando para leer
    i2c_master_send(puertoi2c, &READ_USER_REGISTER, 1);

    // Leer configuración del sensor
    i2c_master_recv(puertoi2c, &sensorConfig, 1);

    pr_info("i2c: Se leyó configuración %#02X\n", sensorConfig);

    switch(resolucionTemperatura)
    {
        case 14:
            // Temperatura 14 bits, humedad 12 bits            
            sensorConfig &= ~(1UL << 7);
            sensorConfig &= ~(1UL << 0);
            break;
        case 13:
            // Temperatura 13 bits, humedad 8 bits
            sensorConfig |= 1UL << 7;
            sensorConfig &= ~(1UL << 0);
            break;            
        case 12:
            // Temperatura 12 bits, humedad 10 bits
            sensorConfig &= ~(1UL << 7);
            sensorConfig |= 1UL << 0;
            break;
        case 11:
            // Temperatura 11 bits, humedad 11 bits
            sensorConfig |= 1UL << 7;
            sensorConfig |= 1UL << 0;
            break;
        default:
            // Temperatura 14 bits, humedad 12 bits            
            sensorConfig &= ~(1UL << 7);
            sensorConfig &= ~(1UL << 0);
            break;
    }

    // Escribe la configuración
    configToWrite[0] = WRITE_USER_REGISTER;
    configToWrite[1] = sensorConfig;

    i2c_master_send(puertoi2c, configToWrite, 2);

    pr_info("i2c: Se seteo configuración %#02X\n", sensorConfig);

    // Enviar comando para leer
    i2c_master_send(puertoi2c, &READ_USER_REGISTER, 1);

    // Leer configuración del sensor
    i2c_master_recv(puertoi2c, &sensorConfigVerif, 1);

    pr_info("i2c: Se leyo configuración %#02X\n", sensorConfigVerif);

    pr_info("i2c: Se seteo resolucion de temperatura %d bits\n", resolucionTemperatura);

   return 0;
}


static int dev_release(struct inode *inodep, struct file *filep){
   //pr_info("EBBChar: Device successfully closed\n");
   return 0;
}

static int pistahtu21d_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
    ebbchar_init();
    puertoi2c = client;     
    return 0;
}




static int pistahtu21d_remove(struct i2c_client *client)
{
	/* unregister device from kernel framework */
	/* shut down the device */
    ebbchar_exit();
    return 0;
}

static struct i2c_driver pistahtu21d_driver = {
    .driver = {
        .name = "pistahtu21d",
        .of_match_table = pistahtu21d_of_match,
    },
    .probe = pistahtu21d_probe,
    .remove = pistahtu21d_remove,
    .id_table = pistahtu21d_id,
};

static int htu21_crc_check( uint16_t value, uint8_t crc)
{
    int res = -1;
	uint32_t polynom = 0x988000; // x^8 + x^5 + x^4 + 1
	uint32_t msb     = 0x800000;
	uint32_t mask    = 0xFF8000;
	uint32_t result  = (uint32_t)value<<8; // Pad with zeros as specified in spec
	
	while( msb != 0x80 ) {
		
		// Check if msb of current value is 1 and apply XOR mask
		if( result & msb )
			result = ((result ^ polynom) & mask) | ( result & ~mask);
			
		// Shift by one
		msb >>= 1;
		mask >>= 1;
		polynom >>=1;
	}

	if( result == crc )
    {
		res = 0;
    }
        
	return res;
}

module_i2c_driver(pistahtu21d_driver);



