const fs = require('fs');

const input = JSON.parse(fs.readFileSync(process.argv[2], 'utf8'));
const outputPath = process.argv[3];

const definitions = [
  ['layer:application', '应用业务层', '承载双传感器数据融合、动作识别与规则分析，并编排 OTA、云端通信和屏幕交互等智能手表业务流程。'],
  ['layer:bsp', '板级设备接入层', '封装 ESP8266、MQTT/OneNET、LT168B 显示屏、W25Q128 Flash 与调试 UART 等外设和网络设备接口。'],
  ['layer:utility', '公共组件层', '提供 CRC16、MD5、环形缓冲区以及 OTA 设备信息、布局与共享数据结构等跨模块基础能力。'],
  ['layer:mcu-runtime', 'MCU 平台与运行时层', '包含 STM32H743 启动入口、FreeRTOS 初始化、中断、HAL 配置以及 GPIO、DMA、定时器、UART、CRC 和内部 Flash 驱动。'],
  ['layer:system-support', 'SYSTEM 支撑层', '保留延时、系统时钟与串口等传统底层适配代码，为现有固件模块提供兼容性支撑。'],
  ['layer:config', '项目配置与分析元数据', '集中管理 CubeMX 工程配置、AI 模型信息以及 Understand Anything 的扫描配置、指纹和知识图谱元数据。'],
  ['layer:documentation', '文档与硬件资料', '汇集项目说明、设计方案、芯片参考笔记、模块引脚表、授权文本及团队协作规范。']
];

function classify(node) {
  const p = (node.filePath || '').replace(/\\/g, '/');
  if (node.type === 'document' || node.type === 'table' || p.startsWith('docs/')) return 'layer:documentation';
  if (p.startsWith('App/')) return 'layer:application';
  if (p.startsWith('BSP/')) return 'layer:bsp';
  if (p.startsWith('Common/')) return 'layer:utility';
  if (p.startsWith('Core/')) return 'layer:mcu-runtime';
  if (p.startsWith('SYSTEM/')) return 'layer:system-support';
  return 'layer:config';
}

const layers = definitions.map(([id, name, description]) => ({ id, name, description, nodeIds: [] }));
const byLayer = new Map(layers.map((layer) => [layer.id, layer]));
for (const node of input.fileNodes) byLayer.get(classify(node)).nodeIds.push(node.id);

const assigned = layers.flatMap((layer) => layer.nodeIds);
const expected = new Set(input.fileNodes.map((node) => node.id));
const duplicates = assigned.filter((id, index) => assigned.indexOf(id) !== index);
const missing = [...expected].filter((id) => !assigned.includes(id));
const invented = assigned.filter((id) => !expected.has(id));
if (layers.length < 3 || layers.length > 10 || layers.some((layer) => !layer.nodeIds.length) || duplicates.length || missing.length || invented.length || assigned.length !== expected.size) {
  throw new Error(JSON.stringify({ layerCount: layers.length, assigned: assigned.length, expected: expected.size, duplicates, missing, invented }));
}
fs.writeFileSync(outputPath, JSON.stringify(layers, null, 2), 'utf8');
console.log(JSON.stringify(layers.map(({ name, nodeIds }) => ({ name, count: nodeIds.length }))));
