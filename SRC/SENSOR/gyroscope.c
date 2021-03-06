/**********************************************************************************************************
                                天穹飞控 —— 致力于打造中国最好的多旋翼开源飞控
                                Github: github.com/loveuav/BlueSkyFlightControl
                                技术讨论：bbs.loveuav.com/forum-68-1.html
 * @文件     gyroscope.c
 * @说明     陀螺仪校准及数据预处理
 * @版本  	 V1.0
 * @作者     BlueSky
 * @网站     bbs.loveuav.com
 * @日期     2018.05 
**********************************************************************************************************/
#include "gyroscope.h"
#include "parameter.h"
#include "accelerometer.h"
#include "faultDetect.h"
#include "flightStatus.h"

GYROSCOPE_t gyro;

static void GyroDetectCheck(Vector3f_t gyroRaw);

/**********************************************************************************************************
*函 数 名: GyroPreTreatInit
*功能说明: 陀螺仪预处理初始化
*形    参: 无
*返 回 值: 无
**********************************************************************************************************/
void GyroPreTreatInit(void)
{
	ParamGetData(PARAM_GYRO_OFFSET_X, &gyro.cali.offset.x, 4);
	ParamGetData(PARAM_GYRO_OFFSET_Y, &gyro.cali.offset.y, 4);
	ParamGetData(PARAM_GYRO_OFFSET_Z, &gyro.cali.offset.z, 4);
    
    if(isnan(gyro.cali.offset.x) || isnan(gyro.cali.offset.y) || isnan(gyro.cali.offset.z))
    {
        gyro.cali.offset.x = 0;
        gyro.cali.offset.y = 0;
        gyro.cali.offset.z = 0;
    }
    
	//陀螺仪低通滤波系数计算
	LowPassFilter2ndFactorCal(0.001, GYRO_LPF_CUT, &gyro.lpf_2nd);	
}

/**********************************************************************************************************
*函 数 名: GyroDataPreTreat
*功能说明: 陀螺仪数据预处理
*形    参: 陀螺仪原始数据 陀螺仪预处理数据指针
*返 回 值: 无
**********************************************************************************************************/
void GyroDataPreTreat(Vector3f_t gyroRaw, Vector3f_t* gyroData, Vector3f_t* gyroLpfData)
{	
	gyro.data = gyroRaw;

    //检测陀螺仪是否工作正常
    GyroDetectCheck(gyroRaw);
	
	//零偏误差校准
	gyro.data.x -= gyro.cali.offset.x;
	gyro.data.y -= gyro.cali.offset.y;	
	gyro.data.z -= gyro.cali.offset.z;	
	
	//安装误差校准
    gyro.data = VectorRotate(gyro.data, GetLevelCalibraData());
 
	//低通滤波
	gyro.dataLpf = LowPassFilter2nd(&gyro.lpf_2nd, gyro.data);
    
    *gyroData = gyro.data;
    *gyroLpfData = gyro.dataLpf;
}

/**********************************************************************************************************
*函 数 名: GyroCalibration
*功能说明: 陀螺仪校准
*形    参: 陀螺仪原始数据 
*返 回 值: 无
**********************************************************************************************************/
void GyroCalibration(Vector3f_t gyroRaw)
{
	const int16_t CALIBRATING_GYRO_CYCLES = 1000;	
	static float gyro_sum[3] = {0, 0, 0};
	Vector3f_t gyro_cali_temp, gyro_raw_temp;
	static int16_t count = 0;
	static uint8_t staticFlag;
	static uint8_t successFlag;
	
	if(!gyro.cali.should_cali)
		return;
	
	gyro_raw_temp = gyroRaw;

	gyro_sum[0] += gyro_raw_temp.x;
	gyro_sum[1] += gyro_raw_temp.y;
	gyro_sum[2] += gyro_raw_temp.z;
	count++;
    
    gyro.cali.step = 1;
    
	//陀螺仪校准过程中如果检测到飞机不是静止状态则认为校准失败
	if(GetPlaceStatus() != STATIC)
	{
		staticFlag = 1;
	}
	
	if(count == CALIBRATING_GYRO_CYCLES)
	{
		count = 0;
		gyro.cali.should_cali = 0;
		gyro.cali.step = 0;     
        
		gyro_cali_temp.x = gyro_sum[0] / CALIBRATING_GYRO_CYCLES;
		gyro_cali_temp.y = gyro_sum[1] / CALIBRATING_GYRO_CYCLES;
		gyro_cali_temp.z = gyro_sum[2] / CALIBRATING_GYRO_CYCLES;
		gyro_sum[0] = 0;
		gyro_sum[1] = 0;
		gyro_sum[2] = 0;
		
		//检测校准数据是否有效		
		if((abs(gyro_raw_temp.x - gyro_cali_temp.x) + abs(gyro_raw_temp.x - gyro_cali_temp.x)
			+ abs(gyro_raw_temp.x - gyro_cali_temp.x)) < 0.6f)
		{
			successFlag = 1;
		}	
		
		if(successFlag && !staticFlag)
		{
			gyro.cali.offset.x = gyro_cali_temp.x;
			gyro.cali.offset.y = gyro_cali_temp.y;
			gyro.cali.offset.z = gyro_cali_temp.z;		
				
			//保存陀螺仪校准参数
			ParamUpdateData(PARAM_GYRO_OFFSET_X, &gyro.cali.offset.x);
			ParamUpdateData(PARAM_GYRO_OFFSET_Y, &gyro.cali.offset.y);
			ParamUpdateData(PARAM_GYRO_OFFSET_Z, &gyro.cali.offset.z);
		}
		
		successFlag = 0;
		staticFlag = 0;		
	}
}

/**********************************************************************************************************
*函 数 名: GyroCalibrateEnable
*功能说明: 陀螺仪校准使能
*形    参: 无 
*返 回 值: 无
**********************************************************************************************************/
void GyroCalibrateEnable(void)
{
	gyro.cali.should_cali = 1;
}

/**********************************************************************************************************
*函 数 名: GetGyroCaliStatus
*功能说明: 陀螺仪校准状态
*形    参: 无 
*返 回 值: 状态 0：不在校准中 非0：校准中
**********************************************************************************************************/
uint8_t GetGyroCaliStatus(void)
{
	return gyro.cali.step;
}

/**********************************************************************************************************
*函 数 名: GyroGetData
*功能说明: 获取经过处理后的陀螺仪数据
*形    参: 无 
*返 回 值: 角速度
**********************************************************************************************************/
Vector3f_t GyroGetData(void)
{
    return gyro.data;
}

/**********************************************************************************************************
*函 数 名: GyroLpfGetData
*功能说明: 获取低通滤波后的陀螺仪数据
*形    参: 无 
*返 回 值: 角速度
**********************************************************************************************************/
Vector3f_t GyroLpfGetData(void)
{
    return gyro.dataLpf;
}

/**********************************************************************************************************
*函 数 名: GyroDetectCheck
*功能说明: 检测陀螺仪工作是否正常，通过检测传感器原始数据变化来判断
*形    参: 陀螺仪原始数据 
*返 回 值: 无
**********************************************************************************************************/
static void GyroDetectCheck(Vector3f_t gyroRaw)
{
	static uint32_t cnt;
	static Vector3f_t lastGyroRaw;
	
	if((gyroRaw.x == lastGyroRaw.x) && (gyroRaw.y == lastGyroRaw.y) && (gyroRaw.z == lastGyroRaw.z))
	{
		cnt++;
		
		if(cnt > 300)
		{
			//未检测到陀螺仪
			FaultDetectSetError(GYRO_UNDETECTED);
		}
	}
	else
	{
		cnt = 0;
		FaultDetectResetError(GYRO_UNDETECTED);
	}
	
	lastGyroRaw = gyroRaw;	
}

