const fs = require('fs');
const path = require('path');

const root = process.argv[2];
const intermediate = path.join(root, '.understand-anything', 'intermediate');
const graphPath = path.join(intermediate, 'assembled-graph.json');
const layersPath = path.join(intermediate, 'layers.json');
const tourPath = path.join(intermediate, 'tour.json');

const graph = JSON.parse(fs.readFileSync(graphPath, 'utf8'));
const layers = JSON.parse(fs.readFileSync(layersPath, 'utf8'));
const tour = JSON.parse(fs.readFileSync(tourPath, 'utf8'));

const assembled = {
  version: '1.0.0',
  project: {
    name: 'H743 Smart Watch',
    languages: ['c', 'csv', 'ioc', 'json', 'markdown', 'txt', 'unknown', 'xlsx'],
    frameworks: ['STM32 HAL', 'FreeRTOS', 'LwIP', 'X-CUBE-AI'],
    description: '基于 STM32H743 的智能手表主控固件，负责双传感器数据融合、动作识别与规则分析，并连接云平台和 LT168B 屏幕。',
    analyzedAt: new Date().toISOString(),
    gitCommitHash: process.argv[3]
  },
  nodes: graph.nodes,
  edges: graph.edges,
  layers,
  tour
};

fs.writeFileSync(graphPath, JSON.stringify(assembled, null, 2) + '\n', 'utf8');
