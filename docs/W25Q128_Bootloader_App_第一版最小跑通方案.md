# W25Q128 + Bootloader/App 第一版最小跑通方案

## 0. 英文术语先对齐

- Bootloader：启动加载程序，复位后第一个运行，负责决定是否跳转到 App。
- App：正式应用程序，也就是当前 H743 主业务工程。
- Flash：这里分两类，内部 Flash 是 STM32H743 片内程序存储区，外部 Flash 是 W25Q128。
- Scatter：Keil/ARMCC 的链接分散加载文件，用来决定程序被链接到哪个 Flash 地址。
- MSP：主栈指针，Cortex-M 启动时从向量表第 0 项读取。
- Reset_Handler：复位入口函数，Cortex-M 启动时从向量表第 1 项读取。
- VTOR：向量表偏移寄存器，决定中断向量表从哪个地址开始取。
- OTA_Meta / OTA_Flag：后续 OTA 用的元信息/状态标志区，第一版只规划，不启用。
- bin：编译后可直接烧写/搬运的裸二进制固件文件。

## 1. 第一版目标和边界

第一版只验证一件事：

```text
复位
  ↓
Bootloader 从 0x08000000 启动
  ↓
Bootloader 打印 BOOT
  ↓
设置 VTOR/MSP
  ↓
跳转 0x08020000
  ↓
App 打印 BOOT OK
  ↓
App 的 FreeRTOS、TIM、USART、DMA 中断正常运行
```

第一版不做 W25Q128 读写、不做 OTA 下载、不做固件搬运、不做回滚、不做复杂 Flag 状态机。

W25Q128 现在只做硬件引脚和分区规划，等 Bootloader -> App 跳转稳定后，再进入第二版。

## 2. 当前工程信息

从当前 H743 工程读取到的关键点：

- 当前 App 的 Keil scatter 仍是 `0x08000000 / 0x00200000`，后续要改成 `0x08020000 / 0x001E0000`。
- 当前 App 已有启动打印：`Debug_Printf("BOOT OK\r\n")`。
- 当前 App 使用 FreeRTOS。
- 当前 App 的 HAL Tick 使用 `TIM1_UP_IRQn`，不是默认 SysTick。
- 当前 App 串口和 DMA 中断较多，重点验证 USART1、USART3、UART4、USART6、UART7、DMA1_Stream0~3。
- `system_stm32h7xx.c` 里 `USER_VECT_TAB_ADDRESS` 当前未启用，所以 App 侧需要显式设置 `SCB->VTOR = 0x08020000`。

调试串口建议第一版优先用 `USART2`：

| 用途 | TX | RX | 原因 |
|---|---|---|---|
| Bootloader 调试打印 | PA2 / USART2_TX | PA3 / USART2_RX | 当前引脚表标注为调试/printf，和屏幕 UART7 不冲突 |
| App 现有打印 | 当前工程已有 `Debug_Printf` | 取决于 `APP_UART7_ROLE` | 如果 UART7 当前给屏幕用，`Debug_Printf` 会被宏关掉 |

注意：当前 `uart7_role.h` 默认 `APP_UART7_ROLE` 是 `UART7_ROLE_SCREEN`，所以如果想看到 App 的 `BOOT OK`，要么把第一版临时改成 UART7 调试角色，要么在 App 里改用 USART2 打印。

## 3. 内部 Flash 分区

```text
0x08000000 - 0x0801FFFF    Bootloader，128KB
0x08020000 - 0x081FFFFF    App 正式运行区，1920KB
```

Bootloader scatter：

```text
LR_IROM1 0x08000000 0x00020000
ER_IROM1 0x08000000 0x00020000
```

App scatter：

```text
LR_IROM1 0x08020000 0x001E0000
ER_IROM1 0x08020000 0x001E0000
```

RAM 第一版不动，沿用当前配置：

```text
RW_IRAM1 0x20000000 0x00020000
RW_IRAM2 0x24000000 0x00080000
```

## 4. 外部 W25Q128 第一版规划

W25Q128 容量是 128Mbit，也就是 16MB，地址范围约为：

```text
0x000000 - 0xFFFFFF
```

第一版先规划为：

```text
0x000000 - 0x00FFFF        OTA_Meta / OTA_Flag 控制区，64KB
0x010000 - 0x20FFFF        新版本 App bin 暂存区，2MB
0x210000 - 0xFFFFFF        预留
```

这套分区暂时不用代码访问，只保证后续设计不冲突。

## 5. W25Q128 引脚建议

参考当前引脚笔记，H743 这块板上已经引出一组 QUADSPI 相关引脚，更适合作为 W25Q128 的长期方案：

| W25Q128 信号 | STM32H743 引脚 | 板上标注 | 说明 |
|---|---|---|---|
| CS# | PG6 或 PB6 | QSPI_CS / D26 | 优先确认焊桥和板上连接，PG6 可能与 USB_FS_PWR_EN 有关系 |
| CLK | PB2 | QSPI_CLK / D27 | 可作为 QSPI 时钟 |
| IO0 / DI | PD11 | QSPI_BK1_IO0 / D30 | QSPI 数据线 0 |
| IO1 / DO | PD12 | QSPI_BK1_IO1 / D29 | QSPI 数据线 1 |
| IO2 / WP# | PE2 | QSPI_BK1_IO2 / D31 | QSPI 数据线 2 |
| IO3 / HOLD# | PD13 | QSPI_BK1_IO3 / D28 | QSPI 数据线 3 |
| VCC | 3V3 | 3.3V | W25Q128 用 3.3V 版本 |
| GND | GND | 地 | 必须共地 |

第一版虽然不写 W25Q128 驱动，但建议硬件接线直接按 QUADSPI 接，避免第二版从软件 SPI 改硬件 QSPI 时返工。

如果第一版只是想用软件 SPI 验证芯片 ID，也可以临时用普通 GPIO 模拟 `CS/SCK/MOSI/MISO`。但当前目标明确是不做 W25Q128 驱动，所以这一版不建议把精力放到软件 SPI 上。

## 6. 参考 W25Q64 代码能借鉴什么

参考例程是 F103 软件 SPI + W25Q64，不能直接搬到 H743，但可以借鉴三层思路：

```text
GPIO 位操作层：
  CS、SCK、MOSI 输出，MISO 输入

SPI 字节交换层：
  模式 0，SCK 空闲低，上升沿采样

Flash 指令层：
  读 JEDEC ID、写使能、读状态寄存器、页编程、4KB 擦除、读数据
```

W25Q128 与 W25Q64 的常用指令大体一致，第二版最小驱动可以从这些命令开始：

```c
#define W25Q128_WRITE_ENABLE            0x06
#define W25Q128_READ_STATUS_REGISTER_1  0x05
#define W25Q128_PAGE_PROGRAM            0x02
#define W25Q128_SECTOR_ERASE_4KB        0x20
#define W25Q128_JEDEC_ID                0x9F
#define W25Q128_READ_DATA               0x03
#define W25Q128_DUMMY_BYTE              0xFF
```

但第一版只保留这个规划，不新增驱动文件。

## 7. Bootloader 最小流程

Bootloader 第一版只初始化必要系统和一个调试串口，然后跳转 App。

推荐流程：

```text
Bootloader_Reset
  ↓
HAL_Init
  ↓
SystemClock_Config
  ↓
MX_GPIO_Init
  ↓
MX_USART2_UART_Init
  ↓
打印 "BOOT"
  ↓
检查 0x08020000 是否像有效 App
  ↓
关闭中断、停 Tick、清 NVIC pending
  ↓
设置 SCB->VTOR = 0x08020000
  ↓
设置 MSP = *(uint32_t *)0x08020000
  ↓
跳转 *(uint32_t *)(0x08020000 + 4)
```

App 有效性第一版可以只做两个轻量判断：

```text
App MSP 是否落在 RAM 范围：
  0x20000000 - 0x2001FFFF
  或 0x24000000 - 0x2407FFFF

App Reset_Handler 是否落在 App Flash 范围：
  0x08020000 - 0x081FFFFF
```

## 8. Bootloader 跳转核心代码模板

```c
#define APP_FLASH_BASE      0x08020000UL
#define APP_FLASH_END       0x081FFFFFUL
#define SRAM1_BASE_ADDR     0x20000000UL
#define SRAM1_END_ADDR      0x2001FFFFUL
#define AXI_SRAM_BASE_ADDR  0x24000000UL
#define AXI_SRAM_END_ADDR   0x2407FFFFUL

typedef void (*app_entry_t)(void);

static int Boot_IsValidApp(uint32_t app_addr)
{
  uint32_t app_msp = *(volatile uint32_t *)app_addr;
  uint32_t app_reset = *(volatile uint32_t *)(app_addr + 4U);
  int msp_ok = 0;
  int reset_ok = 0;

  msp_ok = ((app_msp >= SRAM1_BASE_ADDR) && (app_msp <= SRAM1_END_ADDR)) ||
           ((app_msp >= AXI_SRAM_BASE_ADDR) && (app_msp <= AXI_SRAM_END_ADDR));

  reset_ok = ((app_reset >= APP_FLASH_BASE) && (app_reset <= APP_FLASH_END));

  return (msp_ok && reset_ok);
}

static void Boot_DeInitBeforeJump(void)
{
  uint32_t i;

  __disable_irq();

  SysTick->CTRL = 0;
  SysTick->LOAD = 0;
  SysTick->VAL = 0;

  HAL_SuspendTick();
  HAL_RCC_DeInit();
  HAL_DeInit();

  for (i = 0; i < 8U; i++)
  {
    NVIC->ICER[i] = 0xFFFFFFFFUL;
    NVIC->ICPR[i] = 0xFFFFFFFFUL;
  }
}

void Boot_JumpToApp(void)
{
  uint32_t app_msp;
  uint32_t app_reset;
  app_entry_t app_entry;

  if (!Boot_IsValidApp(APP_FLASH_BASE))
  {
    return;
  }

  app_msp = *(volatile uint32_t *)APP_FLASH_BASE;
  app_reset = *(volatile uint32_t *)(APP_FLASH_BASE + 4U);
  app_entry = (app_entry_t)app_reset;

  Boot_DeInitBeforeJump();

  SCB->VTOR = APP_FLASH_BASE;
  __DSB();
  __ISB();

  __set_MSP(app_msp);
  app_entry();
}
```

这个模板是第一版思路，不建议直接复制后不验证。实际落地时，Bootloader 用过哪些外设，就只反初始化哪些外设，保持最小化。

## 9. App 侧必须做的事

App 链接地址改到 `0x08020000` 后，启动早期要再次设置向量表。

推荐放在 `main()` 最前面，`HAL_Init()` 之前：

```c
#define APP_FLASH_BASE 0x08020000UL

int main(void)
{
  SCB->VTOR = APP_FLASH_BASE;
  __DSB();
  __ISB();

  MPU_Config();
  HAL_Init();
  SystemClock_Config();
  ...
}
```

也可以启用 `system_stm32h7xx.c` 里的 `USER_VECT_TAB_ADDRESS`，并设置：

```c
#define VECT_TAB_BASE_ADDRESS   FLASH_BANK1_BASE
#define VECT_TAB_OFFSET         0x00020000U
```

第一版建议显式写在 `main()` 早期，更直观，方便调试。

## 10. 下载和验证步骤

1. 建一个独立 Bootloader 工程，Flash 起始 `0x08000000`，大小 `0x00020000`。
2. Bootloader 只保留时钟、GPIO、USART2 打印、跳转函数。
3. 当前 App 工程 scatter 改成 `0x08020000 / 0x001E0000`。
4. App 入口早期设置 `SCB->VTOR = 0x08020000`。
5. 先烧 Bootloader 到 `0x08000000`。
6. 再烧 App 到 `0x08020000`。
7. 下载 App 时不要全片擦除，否则会擦掉 Bootloader。
8. 复位后观察串口：

```text
BOOT
BOOT OK
```

9. 再验证 App 的关键中断：

```text
TIM1_UP_IRQn：HAL Tick 正常
TIM2_IRQn：业务定时器正常
USART1/USART3/UART4/USART6/UART7：串口中断正常
DMA1_Stream0~3：DMA 接收正常
FreeRTOS：任务正常调度
```

## 11. 第一版易错点

- App 只改 scatter 不改 VTOR，会出现主循环能跑但中断异常的问题。
- Bootloader 跳转前没有关中断，可能跳过去后执行 Bootloader 的中断现场。
- 当前工程 HAL Tick 用 TIM1，Bootloader 如果也用了 TIM1，跳转前必须停掉。
- App 下载时全片擦除，会把 Bootloader 擦掉。
- UART7 当前默认给屏幕，不一定能看到 `Debug_Printf("BOOT OK")`。
- W25Q128 如果接 QUADSPI 引脚，要提前确认 PG6/PB6 的 CS 焊桥和 USB 相关占用。
- H743 有 I/D Cache 和 MPU，第一版先不在 Bootloader 里开复杂缓存策略，减少变量。

## 12. 第二版再做什么

第一版跑通后，第二版只加最小 W25Q128 能力：

```text
Bootloader 初始化 W25Q128
  ↓
读取 JEDEC ID，确认识别到 W25Q128
  ↓
读取 0x000000 的 OTA_Meta / OTA_Flag
  ↓
仍然不搬运固件
  ↓
继续稳定跳 App
```

第二版判断标准：

```text
复位
  ↓
BOOT
  ↓
W25Q128 ID OK
  ↓
OTA_META READ OK 或 EMPTY
  ↓
跳 App
  ↓
BOOT OK
  ↓
中断和 FreeRTOS 正常
```

