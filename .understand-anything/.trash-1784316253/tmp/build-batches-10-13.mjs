import fs from 'node:fs';
import path from 'node:path';

const root = process.cwd();
const intermediate = path.join(root, '.understand-anything', 'intermediate');
const tmp = path.join(root, '.understand-anything', 'tmp');
const batchData = JSON.parse(fs.readFileSync(path.join(intermediate, 'batches.json'), 'utf8'));

function complexity(lines) {
  if (lines > 200) return 'complex';
  if (lines >= 50) return 'moderate';
  return 'simple';
}

function fileSemantics(file) {
  const p = file.path;
  const n = path.basename(p);
  if (p.endsWith('STM32H743ZIT6_引脚排针精华笔记.md')) return ['整理 STM32H743ZIT6 开发板排针、复用功能及板载资源占用，便于硬件接线和引脚冲突排查。', ['documentation','pinout','hardware-reference']];
  if (p.endsWith('参考手册重点部分.md')) return ['提炼 STM32H743 时钟树、总线、DMA 与 Cache 一致性等参考手册重点，并给出常见问题检查清单。', ['documentation','dma','cache','clock-tree']];
  if (p.endsWith('快速上手.md')) return ['提供 STM32H743 从 CubeMX 建工程到外设调试的学习路线，重点说明手册查阅、内存、Cache、DMA 和时钟配置。', ['documentation','getting-started','stm32h7']];
  if (p.includes('network_action_cnn')) return ['记录动作识别 CNN 的 X-CUBE-AI 转换信息、张量规格、算子与内存占用，供生成代码和部署诊断使用。', ['configuration','x-cube-ai','neural-network','metadata']];
  if (n === '.mxproject') return ['保存 STM32CubeMX 工程的源目录、生成目录和工具链元数据，用于工程代码生成。', ['configuration','cubemx','build-system']];
  if (n === '.understandignore') return ['定义知识图谱扫描时排除的第三方库、生成目录和构建产物，控制分析边界。', ['configuration','analysis-scope','ignore-rules']];
  if (p === 'App/ai/motion_ai.c') return ['实现动作识别 AI 推理流水线，包括窗口预处理、基线与能量计算、跌倒检测、概率平滑及演示覆盖。', ['ai-inference','motion-recognition','signal-processing','x-cube-ai']];
  if (p === 'App/ai/motion_ai.h') return ['声明动作 AI 的标签、状态、结果结构和对外控制接口，供采集及业务任务调用。', ['api','ai-inference','type-definition']];
  if (p === 'App/cloud/cloud_task.c') return ['实现云端 FreeRTOS 任务，管理 Wi-Fi、MQTT 会话、周期上报、下行处理及 OTA 独占暂停。', ['cloud-service','mqtt','freertos','ota']];
  if (p === 'App/cloud/cloud_task.h') return ['声明云任务入口和 OTA 独占协调接口。', ['api','cloud-service','ota']];
  if (p === 'App/motion/motion_frame.h') return ['定义运动传感器帧的数据结构和时间戳等共享字段，作为规则识别与 AI 推理的输入契约。', ['data-model','motion-sensor','type-definition']];
  if (p.endsWith('rule_action_recognizer.c')) return ['实现基于规则的动作识别器，完成基线锁定、预触发缓存、会话采样、特征窗口搜索和模板分类。', ['motion-recognition','rule-engine','signal-processing']];
  if (p.endsWith('rule_action_recognizer.h')) return ['定义规则动作识别器的配置、状态机、采样会话和识别结果接口。', ['api','rule-engine','type-definition']];
  if (p === 'BSP/display/lt168b.c') return ['实现 LT168B 串口屏文本写入、触摸帧解析、接收恢复和 Modbus CRC 校验。', ['bsp','display','uart','protocol-parser']];
  if (p === 'BSP/display/lt168b.h') return ['声明 LT168B 显示与触摸事件接口。', ['api','bsp','display']];
  if (p === 'BSP/w25q128/w25q128.c') return ['实现 W25Q128 SPI Flash 初始化、读写、扇区擦除、忙等待和写后校验。', ['bsp','spi-flash','storage-driver']];
  if (p === 'BSP/w25q128/w25q128.h') return ['声明 W25Q128 存储驱动接口、容量和状态定义。', ['api','bsp','storage-driver']];
  if (p === 'BSP/wifi/ESP8266.c') return ['实现 ESP8266 AT 指令和透明传输驱动，处理收发等待、TCP 建连、IPD 数据及错误恢复。', ['bsp','wifi','at-command','network-driver']];
  if (p === 'BSP/wifi/ESP8266.h') return ['声明 ESP8266 初始化、TCP 连接、数据收发和接收喂入接口。', ['api','bsp','wifi']];
  if (p === 'Common/ota_layout.h') return ['定义外部 Flash OTA 分区、固件镜像和升级元数据布局。', ['ota','memory-layout','type-definition']];
  if (p === 'Common/ring_buffer.c') return ['提供固定容量环形缓冲区的初始化、复位、读写和索引推进实现。', ['utility','ring-buffer','data-structure']];
  if (p === 'Common/ring_buffer.h') return ['声明环形缓冲区结构及读写接口。', ['api','ring-buffer','type-definition']];
  if (p.includes('FreeRTOSConfig')) return ['配置 FreeRTOS 调度、堆、优先级、中断和调试选项。', ['configuration','freertos','rtos']];
  if (p === 'Core/Src/bootloader_main.c') return ['实现 Bootloader 入口，校验应用向量表、释放外设并跳转应用，同时配置最小串口和系统时钟。', ['bootloader','entry-point','firmware-update']];
  if (p === 'Core/Src/bootloader_ota.c') return ['实现 OTA 镜像 MD5 校验、内部 Flash 写入及安装决策流程。', ['bootloader','ota','firmware-update','integrity-check']];
  if (p === 'Core/Src/internal_flash.c') return ['封装 STM32H743 内部 Flash 扇区定位、擦除、写入和校验操作。', ['flash-storage','hal','firmware-update']];
  if (p === 'Core/Src/main.c') return ['应用固件入口，配置 MPU、Cache、系统时钟和生成外设后启动 FreeRTOS 调度。', ['entry-point','system-init','freertos','cubemx']];
  if (p.endsWith('stm32h7xx_it.c')) return ['实现 Cortex-M7 异常和 STM32H7 外设中断处理，包括 DMA、定时器及多路 UART 回调分发。', ['interrupt-handler','hal','uart','dma']];
  if (p.endsWith('system_stm32h7xx.c')) return ['提供 CMSIS 系统初始化、核心时钟更新和 Run0 模式切换逻辑。', ['cmsis','system-init','clock-control']];
  if (p === 'Core/Src/tim.c') return ['配置 TIM2/TIM3 及 PWM，并提供舵机角度到脉宽和门锁控制逻辑。', ['hal','timer','pwm','servo']];
  if (p === 'Core/Src/usart.c') return ['配置多路 UART/USART、GPIO、DMA 与中断，并将接收数据分发到环形缓冲区和协议模块。', ['hal','uart','dma','communication']];
  if (p === 'Core/Src/freertos.c') return ['创建应用 FreeRTOS 对象和任务，连接运动、云端及设备业务执行入口。', ['freertos','task-management','system-init']];
  if (p === 'Core/Src/crc.c') return ['配置 STM32 CRC 外设及其底层时钟资源。', ['hal','crc','peripheral-init']];
  if (p === 'Core/Src/dma.c') return ['配置 DMA 控制器时钟和相关中断优先级。', ['hal','dma','peripheral-init']];
  if (p === 'Core/Src/gpio.c') return ['初始化板级 GPIO 输出、电平和中断引脚。', ['hal','gpio','peripheral-init']];
  if (p.endsWith('stm32h7xx_hal_msp.c')) return ['提供 HAL 全局 MSP 初始化，配置系统级时钟与中断优先级。', ['hal','msp','system-init']];
  if (p.endsWith('stm32h7xx_hal_timebase_tim.c')) return ['使用硬件定时器实现 HAL 1 ms 时间基准及中断配置。', ['hal','timebase','timer']];
  if (p.endsWith('.xlsx')) return n.startsWith('~$') ? ['Microsoft Excel 创建的引脚表临时锁文件，不包含可分析的稳定项目内容。', ['temporary-file','spreadsheet','generated']] : ['汇总 STM32H743ZIT6 引脚、排针映射和复用功能，供硬件连接快速查询。', ['hardware-reference','pinout','spreadsheet']];
  if (p === 'Master.ioc') return ['STM32CubeMX 主工程配置，定义 MCU 引脚、时钟、外设、DMA、中间件和代码生成参数。', ['configuration','cubemx','hardware-config','code-generation']];
  if (p === 'SYSTEM/delay/delay.c') return ['提供面向 STM32H7 的阻塞式微秒和毫秒延时实现，并根据系统时钟初始化计时参数。', ['utility','delay','system-timing']];
  if (p === 'SYSTEM/delay/delay.h') return ['声明系统延时初始化及微秒、毫秒延时接口。', ['api','delay','system-timing']];
  if (p === 'SYSTEM/readme.txt') return ['说明 SYSTEM 目录中时钟、延时和串口基础支持代码的来源与使用范围。', ['documentation','system-support','overview']];
  if (p === 'SYSTEM/sys/sys.c') return ['实现 STM32H7 系统时钟初始化及底层系统支持函数。', ['system-init','clock-control','hal']];
  if (p === 'SYSTEM/sys/sys.h') return ['声明系统时钟配置和底层寄存器辅助接口。', ['api','system-init','clock-control']];
  if (p === 'SYSTEM/usart/yuanzi_usart.c') return ['提供基础串口调试支持和标准输出重定向，适配原子示例代码风格。', ['uart','debugging','stdio-retarget']];
  if (p === 'SYSTEM/usart/yuanzi_usart.h') return ['声明基础调试串口配置和输出接口。', ['api','uart','debugging']];
  if (p.startsWith('Core/Inc/')) return [`声明 ${n.replace('.h','')} 模块的 STM32 HAL 配置、句柄或对外接口。`, ['api','hal','type-definition']];
  return [`实现 ${n} 对应的嵌入式固件功能与项目集成接口。`, ['embedded-firmware','stm32h7','module']];
}

function functionSemantics(name) {
  const lower = name.toLowerCase();
  let action = '实现该模块的核心处理逻辑';
  if (lower.includes('init')) action = '初始化相关硬件、状态或运行资源';
  else if (lower.includes('reset') || lower.includes('clear')) action = '复位内部状态并清理历史数据';
  else if (lower.includes('read')) action = '读取数据并处理边界或设备状态';
  else if (lower.includes('write') || lower.includes('program')) action = '写入数据并执行必要的设备时序或校验';
  else if (lower.includes('erase')) action = '擦除目标存储区域并等待操作完成';
  else if (lower.includes('parse') || lower.includes('process') || lower.includes('handle')) action = '解析输入并驱动对应状态机或事件处理';
  else if (lower.includes('compute') || lower.includes('calc')) action = '计算业务所需的派生量或校验值';
  else if (lower.includes('update')) action = '更新运行状态、统计量或推理结果';
  else if (lower.includes('wait')) action = '等待指定设备状态或响应模式，并处理超时';
  else if (lower.includes('send') || lower.includes('publish') || lower.includes('post')) action = '组织并发送通信数据或业务事件';
  else if (lower.includes('boot') || lower.includes('jump')) action = '执行启动校验、固件安装或应用跳转步骤';
  else if (lower.includes('irq') || lower.includes('handler')) action = '处理异常或外设中断并分发事件';
  else if (name === 'main') action = '完成固件启动初始化并进入主运行流程';
  return `${action}（${name}）。`;
}

function buildBatch(index) {
  const batch = batchData.batches.find((x) => x.batchIndex === index);
  const extracted = JSON.parse(fs.readFileSync(path.join(tmp, `ua-file-extract-results-${index}.json`), 'utf8'));
  const nodes = [];
  const edges = [];
  for (const result of extracted.results) {
    const [summary, tags] = fileSemantics(result);
    const nodeType = result.fileCategory === 'docs' ? 'document' : result.fileCategory === 'config' ? 'config' : 'file';
    const fileId = `${nodeType}:${result.path}`;
    nodes.push({id:fileId,type:nodeType,name:path.basename(result.path),filePath:result.path,summary,tags,complexity:complexity(result.nonEmptyLines)});
    if (result.fileCategory === 'code') {
      const exports = new Set((result.exports || []).map((x) => x.name));
      for (const fn of result.functions || []) {
        const span = fn.endLine - fn.startLine + 1;
        if (span < 10 && !exports.has(fn.name)) continue;
        const id = `function:${result.path}:${fn.name}`;
        nodes.push({id,type:'function',name:fn.name,filePath:result.path,lineRange:[fn.startLine,fn.endLine],summary:functionSemantics(fn.name),tags:['function','embedded-firmware','module-logic'],complexity:complexity(span)});
        edges.push({source:fileId,target:id,type:'contains',direction:'forward',weight:1.0});
        if (exports.has(fn.name)) edges.push({source:fileId,target:id,type:'exports',direction:'forward',weight:0.8});
      }
      for (const cls of result.classes || []) {
        const span = cls.endLine - cls.startLine + 1;
        if (span < 20 && (cls.methods || []).length < 2 && !exports.has(cls.name)) continue;
        const id = `class:${result.path}:${cls.name}`;
        nodes.push({id,type:'class',name:cls.name,filePath:result.path,lineRange:[cls.startLine,cls.endLine],summary:`封装 ${cls.name} 的状态和相关操作。`,tags:['class','data-model','module-logic'],complexity:complexity(span)});
        edges.push({source:fileId,target:id,type:'contains',direction:'forward',weight:1.0});
        if (exports.has(cls.name)) edges.push({source:fileId,target:id,type:'exports',direction:'forward',weight:0.8});
      }
      for (const target of batch.batchImportData[result.path] || []) edges.push({source:fileId,target:`file:${target}`,type:'imports',direction:'forward',weight:0.7});
    }
  }
  return {batch, nodes, edges};
}

function writeBatch(index, graph) {
  for (const name of fs.readdirSync(intermediate)) {
    if (new RegExp(`^batch-${index}(?:-part-\\d+)?\\.json$`).test(name)) fs.rmSync(path.join(intermediate, name));
  }
  if (graph.nodes.length <= 60 && graph.edges.length <= 120) {
    fs.writeFileSync(path.join(intermediate, `batch-${index}.json`), JSON.stringify({nodes:graph.nodes,edges:graph.edges}, null, 2));
    return 1;
  }
  const files = [...graph.batch.files].sort((a,b) => a.path.localeCompare(b.path, 'en')).map((x) => x.path);
  const groups = [];
  let group = [];
  for (const filePath of files) {
    const candidate = new Set([...group, filePath]);
    const candidateNodes = graph.nodes.filter((n) => candidate.has(n.filePath));
    const candidateIds = new Set(candidateNodes.map((n) => n.id));
    const candidateEdges = graph.edges.filter((e) => candidateIds.has(e.source));
    if (group.length && (candidateNodes.length > 60 || candidateEdges.length > 120)) {
      groups.push(group);
      group = [filePath];
    } else {
      group.push(filePath);
    }
  }
  if (group.length) groups.push(group);
  for (let k = 0; k < groups.length; k++) {
    const paths = new Set(groups[k]);
    const nodes = graph.nodes.filter((n) => paths.has(n.filePath));
    const ids = new Set(nodes.map((n) => n.id));
    const edges = graph.edges.filter((e) => ids.has(e.source));
    fs.writeFileSync(path.join(intermediate, `batch-${index}-part-${k + 1}.json`), JSON.stringify({nodes,edges}, null, 2));
  }
  return groups.length;
}

for (const index of [10,11,12,13]) {
  const graph = buildBatch(index);
  const parts = writeBatch(index, graph);
  console.log(JSON.stringify({index,parts,nodes:graph.nodes.length,edges:graph.edges.length}));
}
