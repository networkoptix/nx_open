#ifndef __HI_I2C_H__
#define __HI_I2C_H__

#ifdef __cplusplus
 #if __cplusplus
extern "C" {
 #endif
#endif /* __cplusplus */


/*	
typedef struct hiI2C_DATA_S
{

     unsigned char byte;
     unsigned short word;
     unsigned char block[I2C_SMBUS_BLOCK_MAX + 2];     
} I2C_DATA_S;
*/
typedef struct IA_I2C_DATA_S
{
	unsigned char dev_addr;
	unsigned int reg_addr;
	unsigned int addr_byte;
	unsigned int data;
	unsigned int data_byte;
} I2C_DATA_S;

#define I2C_CMD_WRITE      0x0
#define I2C_CMD_READ       0xf


unsigned int i2c_write(unsigned char dev_addr, unsigned int reg_addr, unsigned int data);
unsigned int i2c_read(unsigned char dev_addr, unsigned int reg_addr);

unsigned int i2c_write_ex(unsigned char dev_addr, unsigned int reg_addr, unsigned int addr_byte, unsigned int data, unsigned int data_byte);
unsigned int i2c_read_ex(unsigned char dev_addr, unsigned int reg_addr, unsigned int addr_byte, unsigned int data_byte);

unsigned int gpio_i2c_write_byte_block(unsigned int I2cNum, char I2cDevAddr, unsigned int I2cRegAddr, unsigned int I2cRegAddrByteNum, unsigned char *DataBuf, unsigned int DataLen);
unsigned int gpio_i2c_read_byte_block(unsigned int I2cNum, char I2cDevAddr, unsigned int I2cRegAddr, unsigned int I2cRegAddrByteNum, unsigned char *DataBuf, unsigned int DataLen);


unsigned char gpio_i2c_read(unsigned char devaddress, unsigned char address);
void gpio_i2c_write(unsigned char devaddress, unsigned char address, unsigned char value);
void gpio_i2c_write_continue(unsigned char chipaddr, unsigned char addr, unsigned char *data, unsigned short len);
void gpio_i2c_read_continue(unsigned char chipaddr, unsigned char addr, unsigned char *data, unsigned short len);
void gpio_i2c_write_burst(unsigned char chipaddr, unsigned char *data, unsigned short len);
void gpio_i2c_write_addr(unsigned char chipaddr, unsigned short wordaddr);
void gpio_i2c_at24c256_read(unsigned char chipaddr, unsigned short addr, unsigned char data);
void gpio_i2c_at24c256_write(unsigned char chipaddr, unsigned short addr, unsigned char data);
void gpio_i2c_read_micom(unsigned char chipaddr, unsigned char addr, unsigned char *data, unsigned short len);	


#ifdef __cplusplus
 #if __cplusplus
}
 #endif
#endif /* __cplusplus */

#endif	/* __HI_I2C_H__ */


