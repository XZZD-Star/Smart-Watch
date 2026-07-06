#include "main.h"
#include "FreeRTOS.h"
#include "task.h"
#include "cmsis_os2.h"
#include "./usart/yuanzi_usart.h"
#include "motion_sensor_pipeline.h"

#include <ctype.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define ALIGN_THRESHOLD_US 3000000ULL

typedef struct
{
    uint8_t  sensor_id;
    uint32_t seq;
    float    yaw;
    float    pitch;
    float    roll;
    int32_t  heart_rate;
    int32_t  spo2;
    int8_t   hr_valid;
    int8_t   spo2_valid;
    uint32_t ppg_fill;
    uint32_t ppg_calc_count;
    uint32_t ppg_pending;
    uint32_t ppg_part_id;
    uint32_t ppg_rev_id;
    uint32_t ppg_int_level;
    uint64_t ts_us;
    uint8_t  valid;
} pose_frame_t;

typedef struct
{
    uint8_t data[RXBUFFERSIZE];
    uint16_t len;
    uint64_t ts_us;
    volatile uint8_t pending;
    volatile uint32_t dropped;
} pose_pending_packet_t;

extern osThreadId_t Task1Handle;

static pose_frame_t upper_frame = {0};
static pose_frame_t fore_frame = {0};

static uint8_t  has_last_seq_u = 0U;
static uint8_t  has_last_seq_f = 0U;
static uint32_t last_seq_u = 0U;
static uint32_t last_seq_f = 0U;
static uint32_t lost_u = 0U;
static uint32_t lost_f = 0U;
static uint32_t disorder_u = 0U;
static uint32_t disorder_f = 0U;
static uint32_t align_fail_count = 0U;

static uint8_t  dwt_ts_inited = 0U;
static uint32_t dwt_cycles_per_us = 1U;
static uint32_t dwt_last_cyccnt = 0U;
static uint64_t dwt_cycle_high = 0U;

static pose_pending_packet_t upper_pending_packet = {0};
static pose_pending_packet_t fore_pending_packet = {0};

float yaw = 0.0f;
float pitch = 0.0f;
float roll = 0.0f;
float yaw2 = 0.0f;
float pitch2 = 0.0f;
float roll2 = 0.0f;

static volatile uint8_t fused_row_ready = 0U;
static motion_fused_frame_t fused_frame = {0};

static char *trim_spaces(char *s);
static int parse_u32_token(char *token, uint32_t *out);
static int parse_i32_token(char *token, int32_t *out);
static int parse_float_token(char *token, float *out);
static uint64_t get_ts_us(void);
static int parse_pose_frame(const uint8_t *buf, uint16_t len, uint8_t default_sensor_id, uint64_t ts_us, pose_frame_t *out);
static void update_seq_stats(uint8_t sensor_id, uint32_t seq);
static void process_pose_packet(const uint8_t *buf, uint16_t len, uint8_t default_sensor_id, uint64_t ts_us);
static uint64_t abs_diff_u64(uint64_t a, uint64_t b);
static void try_emit_fused(void);
static uint8_t take_pending_pose_packet(pose_pending_packet_t *packet, uint8_t *buf, uint16_t *len, uint64_t *ts_us);

static char *trim_spaces(char *s)
{
    char *end = NULL;

    while (*s != '\0' && isspace((unsigned char)*s))
    {
        s++;
    }
    if (*s == '\0')
    {
        return s;
    }

    end = s + strlen(s) - 1;
    while (end > s && isspace((unsigned char)*end))
    {
        *end = '\0';
        end--;
    }

    return s;
}

static int parse_u32_token(char *token, uint32_t *out)
{
    char *endptr = NULL;
    unsigned long value = 0UL;

    token = trim_spaces(token);
    if (*token == '\0')
    {
        return 0;
    }

    value = strtoul(token, &endptr, 10);
    endptr = trim_spaces(endptr);
    if (*endptr != '\0')
    {
        return 0;
    }

    *out = (uint32_t)value;
    return 1;
}

static int parse_i32_token(char *token, int32_t *out)
{
    char *endptr = NULL;
    long value = 0L;

    token = trim_spaces(token);
    if (*token == '\0')
    {
        return 0;
    }

    value = strtol(token, &endptr, 10);
    endptr = trim_spaces(endptr);
    if (*endptr != '\0')
    {
        return 0;
    }

    *out = (int32_t)value;
    return 1;
}

static int parse_float_token(char *token, float *out)
{
    char *endptr = NULL;
    float value = 0.0f;

    token = trim_spaces(token);
    if (*token == '\0')
    {
        return 0;
    }

    value = strtof(token, &endptr);
    endptr = trim_spaces(endptr);
    if (*endptr != '\0')
    {
        return 0;
    }

    *out = value;
    return 1;
}

static uint64_t abs_diff_u64(uint64_t a, uint64_t b)
{
    return (a >= b) ? (a - b) : (b - a);
}

static uint64_t get_ts_us(void)
{
    uint32_t now = 0U;

    if (!dwt_ts_inited)
    {
        CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
        DWT->CYCCNT = 0U;
        DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
        dwt_cycles_per_us = HAL_RCC_GetSysClockFreq() / 1000000U;
        if (dwt_cycles_per_us == 0U)
        {
            dwt_cycles_per_us = 1U;
        }
        dwt_last_cyccnt = 0U;
        dwt_cycle_high = 0ULL;
        dwt_ts_inited = 1U;
    }

    now = DWT->CYCCNT;
    if (now < dwt_last_cyccnt)
    {
        dwt_cycle_high += (1ULL << 32);
    }
    dwt_last_cyccnt = now;

    return (dwt_cycle_high + (uint64_t)now) / (uint64_t)dwt_cycles_per_us;
}

static int parse_pose_frame(
    const uint8_t *buf,
    uint16_t len,
    uint8_t default_sensor_id,
    uint64_t ts_us,
    pose_frame_t *out)
{
    char line[192] = {0};
    char *tokens[15] = {0};
    uint16_t i = 0U;
    uint16_t n = 0U;
    char *cursor = NULL;
    uint16_t token_count = 0U;
    uint32_t sid = default_sensor_id;
    uint32_t seq = 0U;
    float y = 0.0f;
    float p = 0.0f;
    float r = 0.0f;
    int32_t heart_rate = -999;
    int32_t spo2 = -999;
    int32_t hr_valid = 0;
    int32_t spo2_valid = 0;
    uint32_t ppg_fill = 0U;
    uint32_t ppg_calc_count = 0U;
    uint32_t ppg_pending = 0U;
    uint32_t ppg_part_id = 0U;
    uint32_t ppg_rev_id = 0U;
    uint32_t ppg_int_level = 0U;

    for (i = 0U; i < len && n < (sizeof(line) - 1U); i++)
    {
        char c = (char)buf[i];

        if (c == '\0')
        {
            break;
        }
        if (c == '\r' || c == '\n')
        {
            if (n == 0U)
            {
                continue;
            }
            break;
        }
        if ((unsigned char)c < 0x20U && c != '\t')
        {
            continue;
        }
        line[n++] = c;
    }
    line[n] = '\0';
    if (n == 0U)
    {
        return 0;
    }

    cursor = line;
    while ((cursor != NULL) && (token_count < 15U))
    {
        char *comma = strchr(cursor, ',');

        if (comma != NULL)
        {
            *comma = '\0';
            tokens[token_count++] = trim_spaces(cursor);
            cursor = comma + 1;
        }
        else
        {
            tokens[token_count++] = trim_spaces(cursor);
            cursor = NULL;
        }
    }

    if (token_count != 12U)
    {
        return 0;
    }

    if (tokens[0] != NULL && *tokens[0] != '\0')
    {
        if (!parse_u32_token(tokens[0], &sid))
        {
            return 0;
        }
    }
    if (!parse_u32_token(tokens[1], &seq))
    {
        return 0;
    }
    if (!parse_float_token(tokens[2], &y) ||
        !parse_float_token(tokens[3], &p) ||
        !parse_float_token(tokens[4], &r))
    {
        return 0;
    }
    if (!parse_i32_token(tokens[5], &heart_rate) ||
        !parse_i32_token(tokens[6], &hr_valid) ||
        !parse_i32_token(tokens[7], &spo2) ||
        !parse_i32_token(tokens[8], &spo2_valid))
    {
        return 0;
    }

    if (!parse_u32_token(tokens[9], &ppg_fill) ||
        !parse_u32_token(tokens[10], &ppg_calc_count))
    {
        return 0;
    }

    if (!parse_u32_token(tokens[11], &ppg_pending))
    {
        return 0;
    }

    if (sid > MOTION_SENSOR_ID_FORE)
    {
        sid = default_sensor_id;
    }

    out->sensor_id = (uint8_t)sid;
    out->seq = seq;
    out->yaw = y;
    out->pitch = p;
    out->roll = r;
    out->heart_rate = heart_rate;
    out->spo2 = spo2;
    out->hr_valid = (int8_t)hr_valid;
    out->spo2_valid = (int8_t)spo2_valid;
    out->ppg_fill = ppg_fill;
    out->ppg_calc_count = ppg_calc_count;
    out->ppg_pending = ppg_pending;
    out->ppg_part_id = ppg_part_id;
    out->ppg_rev_id = ppg_rev_id;
    out->ppg_int_level = ppg_int_level;
    out->ts_us = ts_us;
    out->valid = 1U;

    return 1;
}

static void update_seq_stats(uint8_t sensor_id, uint32_t seq)
{
    uint32_t *last_seq = NULL;
    uint8_t *has_last = NULL;
    uint32_t *lost = NULL;
    uint32_t *disorder = NULL;

    if (sensor_id == MOTION_SENSOR_ID_UPPER)
    {
        last_seq = &last_seq_u;
        has_last = &has_last_seq_u;
        lost = &lost_u;
        disorder = &disorder_u;
    }
    else
    {
        last_seq = &last_seq_f;
        has_last = &has_last_seq_f;
        lost = &lost_f;
        disorder = &disorder_f;
    }

    if (!(*has_last))
    {
        *last_seq = seq;
        *has_last = 1U;
        return;
    }

    if (seq > (*last_seq + 1U))
    {
        *lost += (seq - *last_seq - 1U);
    }
    else if (seq <= *last_seq)
    {
        (*disorder)++;
    }

    *last_seq = seq;
}

static void try_emit_fused(void)
{
    uint64_t diff = 0ULL;

    if (!upper_frame.valid || !fore_frame.valid)
    {
        return;
    }

    diff = abs_diff_u64(upper_frame.ts_us, fore_frame.ts_us);
    if (diff <= ALIGN_THRESHOLD_US)
    {
        fused_frame.ts_us = upper_frame.ts_us;
        fused_frame.upper_yaw = upper_frame.yaw;
        fused_frame.upper_pitch = upper_frame.pitch;
        fused_frame.upper_roll = upper_frame.roll;
        fused_frame.fore_yaw = fore_frame.yaw;
        fused_frame.fore_pitch = fore_frame.pitch;
        fused_frame.fore_roll = fore_frame.roll;
        fused_frame.seq_u = upper_frame.seq;
        fused_frame.seq_f = fore_frame.seq;
        fused_frame.lost_u = lost_u;
        fused_frame.lost_f = lost_f;
        fused_frame.upper_bio.heart_rate = upper_frame.heart_rate;
        fused_frame.upper_bio.spo2 = upper_frame.spo2;
        fused_frame.upper_bio.hr_valid = upper_frame.hr_valid;
        fused_frame.upper_bio.spo2_valid = upper_frame.spo2_valid;
        fused_frame.upper_bio.ppg_fill = upper_frame.ppg_fill;
        fused_frame.upper_bio.ppg_calc_count = upper_frame.ppg_calc_count;
        fused_frame.upper_bio.ppg_pending = upper_frame.ppg_pending;
        fused_frame.upper_bio.ppg_part_id = upper_frame.ppg_part_id;
        fused_frame.upper_bio.ppg_rev_id = upper_frame.ppg_rev_id;
        fused_frame.upper_bio.ppg_int_level = upper_frame.ppg_int_level;
        fused_frame.fore_bio.heart_rate = fore_frame.heart_rate;
        fused_frame.fore_bio.spo2 = fore_frame.spo2;
        fused_frame.fore_bio.hr_valid = fore_frame.hr_valid;
        fused_frame.fore_bio.spo2_valid = fore_frame.spo2_valid;
        fused_frame.fore_bio.ppg_fill = fore_frame.ppg_fill;
        fused_frame.fore_bio.ppg_calc_count = fore_frame.ppg_calc_count;
        fused_frame.fore_bio.ppg_pending = fore_frame.ppg_pending;
        fused_frame.fore_bio.ppg_part_id = fore_frame.ppg_part_id;
        fused_frame.fore_bio.ppg_rev_id = fore_frame.ppg_rev_id;
        fused_frame.fore_bio.ppg_int_level = fore_frame.ppg_int_level;
        fused_frame.align_fail_count = align_fail_count;
        fused_row_ready = 1U;

        upper_frame.valid = 0U;
        fore_frame.valid = 0U;
    }
}

void MotionSensorPipeline_StorePacketFromIsr(uint8_t sensor_id, const uint8_t *buf, uint16_t len)
{
    pose_pending_packet_t *packet = NULL;
    BaseType_t higher_priority_task_woken = pdFALSE;

    /* 中断里只复制原始包并通知任务，解析和融合放到任务态完成。 */
    if ((buf == NULL) || (len == 0U))
    {
        return;
    }

    if (len > RXBUFFERSIZE)
    {
        len = RXBUFFERSIZE;
    }

    packet = (sensor_id == MOTION_SENSOR_ID_UPPER) ? &upper_pending_packet : &fore_pending_packet;
    if (packet->pending != 0U)
    {
        packet->dropped++;
    }

    memcpy(packet->data, buf, len);
    packet->len = len;
    packet->ts_us = get_ts_us();
    packet->pending = 1U;

    if ((Task1Handle != NULL) && (xTaskGetSchedulerState() == taskSCHEDULER_RUNNING))
    {
        vTaskNotifyGiveFromISR((TaskHandle_t)Task1Handle, &higher_priority_task_woken);
        portYIELD_FROM_ISR(higher_priority_task_woken);
    }
}

uint8_t MotionSensorPipeline_TakeFusedFrame(motion_fused_frame_t *frame)
{
    uint8_t has_frame = 0U;

    if (frame == NULL)
    {
        return 0U;
    }

    /* 融合帧由 pipeline 生成、运动任务消费，这里隐藏内部全局状态。 */
    taskENTER_CRITICAL();
    if (fused_row_ready != 0U)
    {
        *frame = fused_frame;
        fused_row_ready = 0U;
        has_frame = 1U;
    }
    taskEXIT_CRITICAL();

    return has_frame;
}

static uint8_t take_pending_pose_packet(
    pose_pending_packet_t *packet,
    uint8_t *buf,
    uint16_t *len,
    uint64_t *ts_us)
{
    uint8_t has_packet = 0U;

    if ((packet == NULL) || (buf == NULL) || (len == NULL) || (ts_us == NULL))
    {
        return 0U;
    }

    taskENTER_CRITICAL();
    if (packet->pending != 0U)
    {
        *len = packet->len;
        *ts_us = packet->ts_us;
        memcpy(buf, packet->data, packet->len);
        packet->pending = 0U;
        has_packet = 1U;
    }
    taskEXIT_CRITICAL();

    return has_packet;
}

static void process_pose_packet(const uint8_t *buf, uint16_t len, uint8_t default_sensor_id, uint64_t ts_us)
{
    pose_frame_t frame = {0};

    if (!parse_pose_frame(buf, len, default_sensor_id, ts_us, &frame))
    {
        return;
    }

    update_seq_stats(frame.sensor_id, frame.seq);

    if (frame.sensor_id == MOTION_SENSOR_ID_UPPER)
    {
        if (upper_frame.valid)
        {
            align_fail_count++;
        }
        upper_frame = frame;
        yaw = frame.yaw;
        pitch = frame.pitch;
        roll = frame.roll;
    }
    else
    {
        if (fore_frame.valid)
        {
            align_fail_count++;
        }
        fore_frame = frame;
        yaw2 = frame.yaw;
        pitch2 = frame.pitch;
        roll2 = frame.roll;
    }

    try_emit_fused();
}

uint8_t Motion_ProcessPendingPosePackets(void)
{
    uint8_t local_buf[RXBUFFERSIZE];
    uint16_t local_len = 0U;
    uint64_t local_ts_us = 0ULL;
    uint8_t processed = 0U;

    /* 任务态逐包解析，避免 USART1/USART3 中断里做 strtof 等重操作。 */
    if (take_pending_pose_packet(&upper_pending_packet, local_buf, &local_len, &local_ts_us) != 0U)
    {
        process_pose_packet(local_buf, local_len, MOTION_SENSOR_ID_UPPER, local_ts_us);
        processed = 1U;
    }

    if (take_pending_pose_packet(&fore_pending_packet, local_buf, &local_len, &local_ts_us) != 0U)
    {
        process_pose_packet(local_buf, local_len, MOTION_SENSOR_ID_FORE, local_ts_us);
        processed = 1U;
    }

    return processed;
}

void MotionSensorPipeline_Reset(void)
{
    /* start/clear 会清空对齐窗口和统计，确保下一次识别从干净状态开始。 */
    upper_frame = (pose_frame_t){0};
    fore_frame = (pose_frame_t){0};
    upper_pending_packet.pending = 0U;
    upper_pending_packet.len = 0U;
    upper_pending_packet.ts_us = 0ULL;
    upper_pending_packet.dropped = 0U;
    fore_pending_packet.pending = 0U;
    fore_pending_packet.len = 0U;
    fore_pending_packet.ts_us = 0ULL;
    fore_pending_packet.dropped = 0U;

    has_last_seq_u = 0U;
    has_last_seq_f = 0U;
    last_seq_u = 0U;
    last_seq_f = 0U;
    lost_u = 0U;
    lost_f = 0U;
    disorder_u = 0U;
    disorder_f = 0U;
    align_fail_count = 0U;

    yaw = 0.0f;
    pitch = 0.0f;
    roll = 0.0f;
    yaw2 = 0.0f;
    pitch2 = 0.0f;
    roll2 = 0.0f;

    fused_row_ready = 0U;
    fused_frame = (motion_fused_frame_t){0};

    dwt_ts_inited = 0U;
    dwt_last_cyccnt = 0U;
    dwt_cycle_high = 0U;
}
