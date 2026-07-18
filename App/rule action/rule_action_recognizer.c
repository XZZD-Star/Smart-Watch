#include "rule_action_recognizer.h"
#include "motion_ai.h"
#include "motion_app_events.h"
#include "onenet.h"
#include "usart.h"
#include <stdio.h>

volatile uint8_t wanju_flag = 0;
volatile uint8_t shangju_flag = 0;
volatile uint8_t ceping_flag = 0;
volatile uint8_t qianping_flag = 0;

volatile uint16_t wanju_t1 = 0;
volatile uint16_t wanju_t2 = 0;
volatile uint16_t wanju_t3 = 0;
volatile uint16_t shangju_t1 = 0;
volatile uint16_t shangju_t2 = 0;
volatile uint16_t shangju_t3 = 0;
volatile uint16_t ceping_t1 = 0;
volatile uint16_t ceping_t2 = 0;
volatile uint16_t ceping_t3 = 0;
volatile uint16_t qianping_t1 = 0;
volatile uint16_t qianping_t2 = 0;
volatile uint16_t qianping_t3 = 0;


extern motion_fused_frame_t fused_frame;

action_label_t action_label;
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
static void rule_action_debug_print_count(char prefix, uint8_t count);

static int32_t rule_action_label_to_action_id(motion_label_t label)
{
	switch (label)
	{
		case MOTION_LABEL_ELBOW_FLEX:
			return 1;
		case MOTION_LABEL_FRONT_RAISE:
			return 2;
		case MOTION_LABEL_SIDE_RAISE:
			return 3;
		case MOTION_LABEL_SHOULDER_RAISE:
			return 4;
		default:
			return 0;
	}
}

static void rule_action_notify_done(motion_label_t label)
{
	int32_t action_id = rule_action_label_to_action_id(label);
	int32_t action_kind_value = MotionEvents_EncodeActionKind(action_id);

	MotionEvents_RequestTrainingPageRefreshByAction((int32_t)label);
	OneNet_UpdateTrainDisplayByAction((int32_t)label);
	if (action_kind_value != 0)
	{
		MotionEvents_QueueActionKind(action_kind_value);
	}
}

static void rule_action_debug_print_stage(char prefix, uint8_t stage)
{
	char text[32];
	int len;

	if ((stage == 0U) || (stage > 9U))
	{
		return;
	}

	len = snprintf(text,
	               sizeof(text),
	               "%u %cts=%llu\r\n",
	               (unsigned int)stage,
	               prefix,
	               (unsigned long long)(fused_frame.ts_us / 1000ULL));
	if (len <= 0)
	{
		return;
	}

	if (len >= (int)sizeof(text))
	{
		len = (int)sizeof(text) - 1;
	}

	(void)HAL_UART_Transmit(&huart2, (uint8_t *)text, (uint16_t)len, 20U);
}

static void rule_action_debug_print_count(char prefix, uint8_t count)
{
	uint8_t text[15];
	uint16_t len = 0U;

	text[len++] = (uint8_t)prefix;
	text[len++] = (uint8_t)'c';
	text[len++] = (uint8_t)'o';
	text[len++] = (uint8_t)'u';
	text[len++] = (uint8_t)'n';
	text[len++] = (uint8_t)'t';
	text[len++] = (uint8_t)' ';
	text[len++] = (uint8_t)'=';
	text[len++] = (uint8_t)' ';

	if (count >= 100U)
	{
		count = 99U;
	}

	if (count >= 10U)
	{
		text[len++] = (uint8_t)('0' + (count / 10U));
		text[len++] = (uint8_t)('0' + (count % 10U));
	}
	else
	{
		text[len++] = (uint8_t)('0' + count);
	}

	text[len++] = (uint8_t)'\r';
	text[len++] = (uint8_t)'\n';
	(void)HAL_UART_Transmit(&huart2, text, len, 20U);
}

void RuleActionRecognizer_Reset(void)
{
	wanju_flag = 0;
	shangju_flag = 0;
	ceping_flag = 0;
	qianping_flag = 0;
	wanju_t1 = 0;
	wanju_t2 = 0;
	wanju_t3 = 0;
	shangju_t1 = 0;
	shangju_t2 = 0;
	shangju_t3 = 0;
	ceping_t1 = 0;
	ceping_t2 = 0;
	ceping_t3 = 0;
	qianping_t1 = 0;
	qianping_t2 = 0;
	qianping_t3 = 0;
	action_label.wanju = 0;
	action_label.shangju = 0;
	action_label.ceping = 0;
	action_label.qianping = 0;
}

void RuleActionRecognizer_Process(void)
{
	if(ceping_t1 == 4)
	{
		ceping_t1 = 0;
		ceping_flag = 1;
		qianping_flag = 0;
		rule_action_debug_print_stage('c', 1U);
	}
	if(qianping_t1 == 3)
	{
		qianping_t1 = 0;
		qianping_flag = 1;
		ceping_flag = 0;
		rule_action_debug_print_stage('q', 1U);
	}
	if(ceping_t2 == 3)
	{
		ceping_t2 = 0;
		ceping_flag = 2;
		wanju_flag = 0;
		shangju_flag = 0;
		rule_action_debug_print_stage('c', 2U);
	}
	if(qianping_t2 == 3)
	{
		qianping_t2 = 0;
		qianping_flag = 2;
		rule_action_debug_print_stage('q', 2U);
	}
		if(ceping_t3 == 2)
	{
		ceping_t3 = 0;
		ceping_flag = 3;
		rule_action_debug_print_stage('c', 3U);
	}
	if(qianping_t3 == 2)
	{
		qianping_t3 = 0;
		qianping_flag = 3;
		rule_action_debug_print_stage('q', 3U);
	}
	if(ceping_flag == 3)
	{
		ceping_flag = 0;
		if(action_label.ceping < 20)
		{
			action_label.ceping++;
			rule_action_debug_print_count('c', action_label.ceping);
		}	
		rule_action_notify_done(MOTION_LABEL_SIDE_RAISE);
	}
	if(qianping_flag == 3)
	{
		qianping_flag = 0;
		if(action_label.qianping < 20)
		{
			action_label.qianping++;
			rule_action_debug_print_count('q', action_label.qianping);
		}	
		rule_action_notify_done(MOTION_LABEL_FRONT_RAISE);
	}
	if(shangju_t1 == 4)
	{
		shangju_t1 = 0;
		shangju_flag = 1;
	}
	if(shangju_t2 == 3)
	{
		shangju_t2 = 0;
		shangju_flag = 2;
		ceping_flag = 0;
		wanju_flag = 0;
	}
	if(shangju_t3 == 2)
	{
		shangju_t3 = 0;
		shangju_flag = 3;
	}
	if(shangju_flag == 3)
	{
		shangju_flag = 0;
		if(action_label.shangju < 15)
		{
			action_label.shangju++;
			rule_action_debug_print_count('s', action_label.shangju);
		}
	}
	
	
	if(wanju_t1 == 4)
	{
		wanju_t1 = 0;
		wanju_flag = 1;
	}
	if(wanju_t2 == 3)
	{
		wanju_t2 = 0;
		wanju_flag = 2;
		shangju_flag = 0;
		ceping_flag = 0;
	}
	if(wanju_t3 == 2)
	{
		wanju_t3 = 0;
		wanju_flag = 3;
	}
	if(wanju_flag == 3)
	{
		wanju_flag = 0;
		if(action_label.wanju < 15)
		{
			action_label.wanju++;
			rule_action_debug_print_count('w', action_label.wanju);
		}
	}	
	
}

void AI_task(void)
{
	RuleActionRecognizer_Process();
}


void diedao_task(void)
{
	
}
