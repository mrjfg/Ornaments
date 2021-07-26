#include "math.h"
#include "stdio.h"
#include "stdlib.h"

#include "mpu6050.h"
i2c_config_t conf;
i2c_port_t g_port;

#define ORIGINAL_OUTPUT			 (0)
#define ACC_FULLSCALE        	 (2)
#define GYRO_FULLSCALE			 (250)


    float gyro_y;
    float angle = 0;
    float q_bias = 0;
    float angle_err = 0;
    float q_angle = 0.1;
    float q_gyro = 0.1;
    float r_angle = 0.5;
    float dt = 0.005;
    char c_0 = 1;
    float pct_0=0, pct_1=0, e=0;
    float k_0=0, k_1=0, t_0=0, t_1=0;
    float pdot[4] = {0, 0, 0, 0};
    float pp[2][2] = {{1, 0}, {0, 1}};

#if ORIGINAL_OUTPUT == 0
	#if  ACC_FULLSCALE  == 2
		#define AccAxis_Sensitive (float)(16384)
	#elif ACC_FULLSCALE == 4
		#define AccAxis_Sensitive (float)(8192)
	#elif ACC_FULLSCALE == 8
		#define AccAxis_Sensitive (float)(4096)
	#elif ACC_FULLSCALE == 16
		#define AccAxis_Sensitive (float)(2048)
	#endif 
		
	#if   GYRO_FULLSCALE == 250
		#define GyroAxis_Sensitive (float)(131.0)
	#elif GYRO_FULLSCALE == 500
		#define GyroAxis_Sensitive (float)(65.5)
	#elif GYRO_FULLSCALE == 1000
		#define GyroAxis_Sensitive (float)(32.8)
	#elif GYRO_FULLSCALE == 2000
		#define GyroAxis_Sensitive (float)(16.4)
	#endif
		
#else
	#define AccAxis_Sensitive  (1)
	#define GyroAxis_Sensitive (1)
#endif


void I2C(gpio_num_t scl, gpio_num_t sda, i2c_port_t port) {
    g_port = port;
    conf.mode = I2C_MODE_MASTER;
    conf.sda_io_num = sda;
    conf.sda_pullup_en = GPIO_PULLUP_ENABLE;
    conf.scl_io_num = scl;
    conf.scl_pullup_en = GPIO_PULLUP_ENABLE;
    conf.master.clk_speed = 400000;
    i2c_param_config(port, &conf);
    i2c_driver_install(port, conf.mode, 0, 0, 0);
}


bool slave_write(uint8_t slave_addr,uint8_t reg_addr, uint8_t data) {
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, slave_addr << 1, 1);
    i2c_master_write_byte(cmd, reg_addr, 1);
    i2c_master_write_byte(cmd, data, 1);
    i2c_master_stop(cmd);
    int ret = i2c_master_cmd_begin(g_port, cmd, 1000 / portTICK_RATE_MS);
    i2c_cmd_link_delete(cmd);
    if (ret == ESP_FAIL) {
        return false;
    }
    return true;
}

bool slave_read(uint8_t slave_addr, uint8_t data, uint8_t *buf, uint32_t len) {
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, slave_addr << 1, 1);
    i2c_master_write_byte(cmd, data, 1);
    i2c_master_stop(cmd);
    int ret = i2c_master_cmd_begin(g_port, cmd, 1000 / portTICK_RATE_MS);
    i2c_cmd_link_delete(cmd);
    if (ret == ESP_FAIL) {
        return false;
    }

    cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, slave_addr << 1 | 1, 1);
    while(len) {
        i2c_master_read_byte(cmd, buf, ((i2c_ack_type_t)(len == 1)));
        buf++;
        len--;
    }
    i2c_master_stop(cmd);
    ret = i2c_master_cmd_begin(g_port, cmd, 1000 / portTICK_RATE_MS);
    i2c_cmd_link_delete(cmd);
    if (ret == ESP_FAIL) {
        return false;
    }

    return true;
}

uint8_t slave_read_byte(uint8_t slave_addr, uint8_t reg) {
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, slave_addr << 1, 1);
    i2c_master_write_byte(cmd, reg, 1);
    i2c_master_stop(cmd);
    i2c_master_cmd_begin(g_port, cmd, 1000 / portTICK_RATE_MS);
    i2c_cmd_link_delete(cmd);

    uint8_t buf;
    cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, slave_addr << 1 | 1, 1);
    i2c_master_read_byte(cmd, &buf, (i2c_ack_type_t)1);
    i2c_master_stop(cmd);
    i2c_master_cmd_begin(g_port, cmd, 1000 / portTICK_RATE_MS);
    i2c_cmd_link_delete(cmd);
    return buf;
}



void MPU6050(gpio_num_t scl, gpio_num_t sda, i2c_port_t port) {
    I2C(scl, sda, port);
}


bool  init() {
    if (! slave_write(MPU6050_ADDR, PWR_MGMT_1  , 0x00))
        return false;
    if (! slave_write(MPU6050_ADDR, SMPLRT_DIV  , 0x07))
        return false;
    if (! slave_write(MPU6050_ADDR, CONFIG      , 0x07))
        return false;
    if (! slave_write(MPU6050_ADDR, GYRO_CONFIG , 0x18))
        return false;
    if (! slave_write(MPU6050_ADDR, ACCEL_CONFIG, 0x01))
        return false;
    return true;
}

float getAccX() {
    uint8_t r[0];
     slave_read(MPU6050_ADDR, ACCEL_XOUT_H, r, 2);
    short accx = r[0] << 8 | r[1];
    return (float)accx / AccAxis_Sensitive;
}

float  getAccY() {
    uint8_t r[0];
     slave_read(MPU6050_ADDR, ACCEL_YOUT_H, r, 2);
    short accy = r[0] << 8 | r[1];
    return (float)accy / AccAxis_Sensitive;
}

float  getAccZ() {
    uint8_t r[0];
     slave_read(MPU6050_ADDR, ACCEL_ZOUT_H, r, 2);
    short accz = r[0] << 8 | r[1];
    return (float)accz / AccAxis_Sensitive;
}

float  getGyroX() {
    uint8_t r[0];
     slave_read(MPU6050_ADDR, GYRO_XOUT_H, r, 2);
    short gyrox = r[0] << 8 | r[1];
    return (float)gyrox / GyroAxis_Sensitive;
}

float  getGyroY() {
    uint8_t r[0];
     slave_read(MPU6050_ADDR, GYRO_YOUT_H, r, 2);
    short gyroy = r[0] << 8 | r[1];
    return (float)gyroy / GyroAxis_Sensitive;
}

float  getGyroZ() {
    uint8_t r[0];
     slave_read(MPU6050_ADDR, GYRO_ZOUT_H, r, 2);
    short gyroz = r[0] << 8 | r[1];
    return (float)gyroz / GyroAxis_Sensitive;
}

short  getTemp() {
    uint8_t r[0];
     slave_read(MPU6050_ADDR, TEMP_OUT_H, r, 2);
    return r[0] << 8 | r[1];
}




float filter(float accel, float gyro ,float dt) {
    angle += (gyro - q_bias) * dt;
    angle_err = accel - angle;

    pdot[0] = q_angle - pp[0][1] - pp[1][0];
    pdot[1] = -pp[1][1];
    pdot[2] = -pp[1][1];
    pdot[3] = q_gyro;
    pp[0][0] += pdot[0] * dt;
    pp[0][1] += pdot[1] * dt;
    pp[1][0] += pdot[2] * dt;
    pp[1][1] += pdot[3] * dt;

    pct_0 = c_0 * pp[0][0];
    pct_1 = c_0 * pp[1][0];

    e = r_angle + c_0 * pct_0;

    k_0 = pct_0 / e;
    k_1 = pct_1 / e;

    t_0 = pct_0;
    t_1 = c_0 * pp[0][1];

    pp[0][0] -= k_0 * t_0;
    pp[0][1] -= k_0 * t_1;
    pp[1][0] -= k_1 * t_0;
    pp[1][1] -= k_1 * t_1;

    angle += k_0 * angle_err;
    q_bias += k_1 * angle_err;
    gyro_y= gyro - q_bias;
    
    return angle;
}

float r_filter(float accel, float gyro ,float dt) {
    angle += (gyro - q_bias) * dt;
    angle_err = accel - angle;

    pdot[0] = q_angle - pp[0][1] - pp[1][0];
    pdot[1] = -pp[1][1];
    pdot[2] = -pp[1][1];
    pdot[3] = q_gyro;
    pp[0][0] += pdot[0] * dt;
    pp[0][1] += pdot[1] * dt;
    pp[1][0] += pdot[2] * dt;
    pp[1][1] += pdot[3] * dt;

    pct_0 = c_0 * pp[0][0];
    pct_1 = c_0 * pp[1][0];

    e = r_angle + c_0 * pct_0;

    k_0 = pct_0 / e;
    k_1 = pct_1 / e;

    t_0 = pct_0;
    t_1 = c_0 * pp[0][1];

    pp[0][0] -= k_0 * t_0;
    pp[0][1] -= k_0 * t_1;
    pp[1][0] -= k_1 * t_0;
    pp[1][1] -= k_1 * t_1;

    angle += k_0 * angle_err;
    q_bias += k_1 * angle_err;
    gyro_y= gyro - q_bias;
    
    return angle;
}

