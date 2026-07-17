const fs = require('fs');
const path = require('path');

const root = 'D:/Qian Start/Smart Watch/H743';
const intermediate = path.join(root, '.understand-anything/intermediate');
const graph = JSON.parse(fs.readFileSync(path.join(intermediate, 'assembled-graph.json'), 'utf8'));
const fileTypes = new Set(['file', 'config', 'document', 'service', 'pipeline', 'table', 'schema', 'resource', 'endpoint']);
const fileNodes = graph.nodes.filter(n => fileTypes.has(n.type));
const byId = new Map(graph.nodes.map(n => [n.id, n]));

function p(node) {
  return (node.filePath || '').replace(/\\/g, '/');
}

const layers = [
  {
    id: 'layer:motion-ai',
    name: '运动识别与 AI 层',
    description: '包含运动数据窗口、融合管线、AI 推理和规则动作识别，是手表动作识别业务的核心。',
    nodeIds: []
  },
  {
    id: 'layer:screen-cloud-ota',
    name: '屏幕云端与 OTA 层',
    description: '连接 LT168B 屏幕、OneNET/MQTT 云端通信和 OTA 版本更新流程，负责用户可见状态与联网业务。',
    nodeIds: []
  },
  {
    id: 'layer:bsp-drivers',
    name: 'BSP 外设驱动层',
    description: '封装屏幕、传感器、舵机和 MQTT 协议等板级能力，向应用层提供硬件和协议接口。',
    nodeIds: []
  },
  {
    id: 'layer:core-system',
    name: 'Core 与系统平台层',
    description: '包含 STM32 启动、FreeRTOS、外设初始化、中断和系统底层支持，是固件运行基础。',
    nodeIds: []
  },
  {
    id: 'layer:common-support',
    name: '通用支持层',
    description: '提供跨模块复用的通用工具、数据结构和轻量公共能力。',
    nodeIds: []
  },
  {
    id: 'layer:project-config-docs',
    name: '项目配置与文档层',
    description: '保存工程配置、模型配置、说明文档和 understand-anything 图谱配置。',
    nodeIds: []
  }
];

function assignLayer(node) {
  const filePath = p(node);
  if (filePath.startsWith('App/motion/') || filePath.startsWith('App/ai/') || filePath.startsWith('App/rule action/')) return layers[0];
  if (filePath.startsWith('App/lt_screen/') || filePath.startsWith('App/cloud/') || filePath.startsWith('App/ota/')) return layers[1];
  if (filePath.startsWith('BSP/')) return layers[2];
  if (filePath.startsWith('Core/') || filePath.startsWith('SYSTEM/')) return layers[3];
  if (filePath.startsWith('Common/')) return layers[4];
  return layers[5];
}

for (const node of fileNodes) {
  assignLayer(node).nodeIds.push(node.id);
}

const nonEmptyLayers = layers.filter(layer => layer.nodeIds.length > 0);
fs.writeFileSync(path.join(intermediate, 'layers.json'), JSON.stringify(nonEmptyLayers, null, 2));

function keep(ids) {
  return ids.filter(id => byId.has(id)).slice(0, 5);
}

const tourDraft = [
  {
    title: '项目约定入口',
    description: '先从项目约定、CubeMX 工程配置和图谱配置看起，可以快速理解这个仓库的边界：当前图谱聚焦自编固件代码，排除了 HAL/CMSIS 和中间件库。这个步骤帮助新人先建立“哪些文件值得读、哪些是生成或外部依赖”的判断。',
    nodeIds: keep(['document:Agents.md', 'config:Master.ioc', 'config:.understand-anything/config.json'])
  },
  {
    title: '固件启动主线',
    description: 'Core 层负责 MCU 启动后的主控路径，包括主入口、FreeRTOS 调度和关键外设初始化。理解这里能知道应用任务何时开始运行，以及后续屏幕、运动、云端任务依赖哪些底层句柄。',
    nodeIds: keep(['file:Core/Src/main.c', 'file:Core/Src/freertos.c', 'file:Core/Src/usart.c', 'file:Core/Src/gpio.c'])
  },
  {
    title: '传感器输入链路',
    description: '运动帧结构和融合管线描述了上层如何接收、缓存并对齐姿态数据。这个步骤连接底层采样结果和动作识别任务，是排查数据时序、采样缺失和融合阈值问题的起点。',
    nodeIds: keep(['file:App/motion/motion_frame.h', 'file:App/motion/motion_sensor_pipeline.c', 'file:App/motion/motion_sensor_pipeline.h'])
  },
  {
    title: '运动任务调度',
    description: '运动任务把输入请求、融合帧、窗口测试和结果事件串在一起。它是本项目运动识别链路的调度中心，理解它可以看清本地训练结果、云端测试值和屏幕刷新事件之间的控制关系。',
    nodeIds: keep(['file:App/motion/motion_task.c', 'file:App/motion/motion_input.c', 'file:App/motion/motion_window_test.c', 'file:App/motion/motion_app_events.c'])
  },
  {
    title: 'AI 与规则识别',
    description: 'AI 推理模块和规则识别器共同决定动作识别结果：一个负责模型推理控制，一个负责模板匹配、峰值保持和完整度评分等规则判断。读这一组文件可以理解动作标签如何从原始窗口数据变成业务结果。',
    nodeIds: keep(['file:App/ai/motion_ai.c', 'file:App/rule action/rule_action_recognizer.c'])
  },
  {
    title: 'LT168B 屏幕显示',
    description: '屏幕任务集中处理触摸键值、页面状态和业务结果渲染，底层 LT168B 驱动负责协议帧发送和地址写入。这里是运动、OTA、跌倒告警等状态最终呈现给用户的出口。',
    nodeIds: keep(['file:App/lt_screen/lt_screen_task.c', 'file:App/lt_screen/lt_screen_keys.h', 'file:BSP/display/lt168b.c'])
  },
  {
    title: '云端通信路径',
    description: '云端任务、OneNET 封装和 MQTT 报文工具共同构成联网链路。这个步骤适合顺着“属性上报、命令下发、连接保持”的方向阅读，定位云端状态如何进入本地业务。',
    nodeIds: keep(['file:App/cloud/cloud_task.c', 'file:BSP/cloud/onenet.c', 'file:BSP/cloud/MqttKit.c'])
  },
  {
    title: 'OTA 版本流程',
    description: 'OTA 服务负责版本查询、模拟升级和升级状态，HTTP 模块则处理请求发送。它与屏幕层和云端独占状态相互配合，是理解版本页、升级按钮和联网升级流程的关键路径。',
    nodeIds: keep(['file:App/ota/ota_service.c', 'file:App/ota/ota_http.c', 'file:App/ota/ota_config.h'])
  },
  {
    title: '系统支撑与工具',
    description: 'SYSTEM 和 Common 中的文件提供延时、调试、内存或轻量工具能力，虽然不直接表达业务，但会影响所有上层模块的运行稳定性。最后读这一层，有助于把应用逻辑和底层支撑条件对应起来。',
    nodeIds: keep(['file:SYSTEM/delay/delay.c', 'file:SYSTEM/usart/yuanzi_usart.c', 'file:Common/ring_buffer.c', 'file:Common/crc16.c'])
  }
];

const tour = tourDraft
  .filter(step => step.nodeIds.length > 0)
  .map((step, index) => ({ order: index + 1, ...step }));

fs.writeFileSync(path.join(intermediate, 'tour.json'), JSON.stringify(tour, null, 2));

console.log(`layers=${nonEmptyLayers.length}, fileNodes=${fileNodes.length}`);
for (const layer of nonEmptyLayers) console.log(`${layer.name}: ${layer.nodeIds.length}`);
console.log(`tourSteps=${tour.length}`);
