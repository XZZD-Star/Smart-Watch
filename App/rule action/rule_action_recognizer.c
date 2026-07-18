#include "rule_action_recognizer.h"
#include "motion_ai.h"
#include "motion_app_events.h"
#include "onenet.h"
#include "usart.h"

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

static void rule_action_debug_print_stage(uint8_t stage)
{
	uint8_t text[3];

	if ((stage == 0U) || (stage > 9U))
	{
		return;
	}

	text[0] = (uint8_t)('0' + stage);
	text[1] = (uint8_t)'\r';
	text[2] = (uint8_t)'\n';
	(void)HAL_UART_Transmit(&huart2, text, (uint16_t)sizeof(text), 20U);
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
		rule_action_debug_print_stage(1U);
	}
	if(qianping_t1 == 4)
	{
		qianping_t1 = 0;
		qianping_flag = 1;
		rule_action_debug_print_stage(1U);
	}
	if(ceping_t2 == 3)
	{
		ceping_t2 = 0;
		ceping_flag = 2;
		rule_action_debug_print_stage(2U);
	}
	if(qianping_t2 == 3)
	{
		qianping_t2 = 0;
		qianping_flag = 2;
		rule_action_debug_print_stage(2U);
	}
		if(ceping_t3 == 2)
	{
		ceping_t3 = 0;
		ceping_flag = 3;
		rule_action_debug_print_stage(3U);
	}
	if(qianping_t3 == 2)
	{
		qianping_t3 = 0;
		qianping_flag = 3;
		rule_action_debug_print_stage(3U);
	}
	if(ceping_flag == 3)
	{
		ceping_flag = 0;
		if(action_label.ceping < 20)
		{
			action_label.ceping++;
		}	
		rule_action_notify_done(MOTION_LABEL_SIDE_RAISE);
	}
	if(qianping_flag == 3)
	{
		qianping_flag = 0;
		if(action_label.qianping < 20)
		{
			action_label.qianping++;
		}	
		rule_action_notify_done(MOTION_LABEL_FRONT_RAISE);
	}
}

void AI_task(void)
{
	RuleActionRecognizer_Process();
}
