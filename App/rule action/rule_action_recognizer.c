#include "rule_action_recognizer.h"
#include ""

uint8_t wanju_flag = 0;
uint8_t shangju_flag = 0;
uint8_t ceping_flag = 0;
uint8_t qianping_flag = 0;

uint16_t wanju_t1 = 0;
uint16_t wanju_t2 = 0;
uint16_t wanju_t3 = 0;
uint16_t shangju_t1 = 0;
uint16_t shangju_t2 = 0;
uint16_t shangju_t3 = 0;
uint16_t ceping_t1 = 0;
uint16_t ceping_t2 = 0;
uint16_t ceping_t3 = 0;
uint16_t qianping_t1 = 0;
uint16_t qianping_t2 = 0;
uint16_t qianping_t3 = 0;


extern motion_fused_frame_t fused_frame;

/*

typedef enum
{
  MOTION_LABEL_REST = 0,
  MOTION_LABEL_ELBOW_FLEX,		//wanju
  MOTION_LABEL_FRONT_RAISE,		//pianping
  MOTION_LABEL_SIDE_RAISE,		//ceping
  MOTION_LABEL_SHOULDER_RAISE,		//shangju
  MOTION_LABEL_UNKNOWN
} motion_label_t;

// action_label

*/
void AI_task(void)
{
	if(ceping_t1 == 4)
	{
		ceping_t1 = 0;
		ceping_flag = 1;
	}
	if(qianping_t1 == 4)
	{
		qianping_t1 = 0;
		qianping_flag = 1;
	}
	if(ceping_t2 == 3)
	{
		ceping_t2 = 0;
		ceping_flag = 2;
	}
	if(qianping_t2 == 3)
	{
		qianping_t2 = 0;
		qianping_flag = 2;
	}
		if(ceping_t3 == 2)
	{
		ceping_t3 = 0;
		ceping_flag = 3;
	}
	if(qianping_t3 == 2)
	{
		qianping_t3 = 0;
		qianping_flag = 3;
	}
	if(ceping_flag == 3)
	{
		ceping_flag = 0;
		
	}
	if(qianping_flag == 3)
	{
		qianping_flag = 0;
		
	}
}

