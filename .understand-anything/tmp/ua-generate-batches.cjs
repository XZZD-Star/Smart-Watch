const fs = require('fs');
const path = require('path');
const cp = require('child_process');

const projectRoot = 'D:/Qian Start/Smart Watch/H743';
const skillDir = 'C:/Users/33709/.understand-anything/repo/understand-anything-plugin/skills/understand';
const intermediateDir = path.join(projectRoot, '.understand-anything/intermediate');
const tmpDir = path.join(projectRoot, '.understand-anything/tmp');
const batches = JSON.parse(fs.readFileSync(path.join(intermediateDir, 'batches.json'), 'utf8')).batches;

function normalizePath(p) {
  return p.replace(/\\/g, '/');
}

function baseName(p) {
  return normalizePath(p).split('/').pop();
}

function topDir(p) {
  return normalizePath(p).split('/')[0] || 'root';
}

function moduleName(filePath) {
  const p = normalizePath(filePath);
  if (p.startsWith('App/motion/')) return '运动采集与识别';
  if (p.startsWith('App/ai/')) return 'AI 推理';
  if (p.startsWith('App/lt_screen/')) return 'LT168B 屏幕交互';
  if (p.startsWith('App/cloud/')) return '云端通信';
  if (p.startsWith('App/ota/')) return 'OTA 升级';
  if (p.startsWith('App/rule action/')) return '规则动作识别';
  if (p.startsWith('BSP/cloud/')) return 'MQTT 协议封装';
  if (p.startsWith('BSP/display/')) return '显示屏驱动';
  if (p.startsWith('BSP/MPU/')) return 'IMU 传感器驱动';
  if (p.startsWith('BSP/QMI8658/')) return 'QMI8658 传感器驱动';
  if (p.startsWith('BSP/servo/')) return '舵机控制';
  if (p.startsWith('Common/')) return '通用工具';
  if (p.startsWith('Core/')) return 'STM32 工程入口与外设初始化';
  if (p.startsWith('SYSTEM/')) return '系统底层支持';
  if (p.startsWith('docs/')) return '项目文档';
  if (p.startsWith('.understand-anything/')) return '图谱配置';
  if (p.startsWith('.ai/')) return 'AI 模型配置';
  return '工程根目录';
}

function purpose(filePath) {
  const p = normalizePath(filePath);
  const name = baseName(p).toLowerCase();
  if (name === 'main.c') return '负责固件启动后的主循环、RTOS 启动和应用任务入口组织。';
  if (name === 'freertos.c') return '配置 FreeRTOS 任务、队列或调度相关的系统运行框架。';
  if (name.includes('motion_task')) return '组织运动识别主任务，把传感器融合、AI/规则识别、训练结果和屏幕/云端事件串联起来。';
  if (name.includes('motion_sensor_pipeline')) return '缓存并对齐姿态传感器数据，向运动任务输出融合后的运动帧。';
  if (name.includes('motion_app_events')) return '在运动模块、屏幕刷新和云端上报之间传递测试结果、动作类型和训练页刷新事件。';
  if (name.includes('motion_input')) return '封装运动流程的开始、停止、清除和重启请求状态。';
  if (name.includes('motion_window_test')) return '维护运动窗口测试流程，用于触发和管理一次动作识别测试。';
  if (name.includes('motion_ai')) return '封装运动 AI 模型输入、推理状态和测试/跌倒相关控制接口。';
  if (name.includes('rule_action_recognizer')) return '实现基于规则和模板匹配的动作识别、评分和结果评估逻辑。';
  if (name.includes('lt_screen_task')) return '集中处理 LT168B 屏幕页面、触摸键值、状态刷新和业务模块到屏幕的显示桥接。';
  if (name.includes('lt168b')) return '封装 LT168B 串口屏协议发送、页面切换、文本/数值写入和调试输出。';
  if (name.includes('cloud_task')) return '组织云端连接、MQTT 通信和 OTA 独占状态等网络任务行为。';
  if (name.includes('onenet')) return '封装 OneNET 平台的属性上报、命令解析和云端状态同步。';
  if (name.includes('mqtt')) return '提供 MQTT 报文打包、解析和连接/订阅/发布协议处理。';
  if (name.includes('ota_service')) return '实现 OTA 查询、模拟升级流程、版本状态和升级结果上报。';
  if (name.includes('ota_http')) return '构造并发送 OTA HTTP 请求，支撑版本查询或升级检查。';
  if (name.includes('qmi') || name.includes('icm')) return '驱动惯性传感器并提供姿态/加速度/陀螺仪数据读取接口。';
  if (name.includes('servo')) return '封装舵机或门锁相关控制状态。';
  if (name.includes('gpio') || name.includes('usart') || name.includes('tim') || name.includes('dma')) return '配置 STM32 外设初始化和中断/句柄入口。';
  if (name.endsWith('.h')) return `声明${moduleName(filePath)}模块的对外类型、宏和函数接口。`;
  if (p.endsWith('.md') || p.endsWith('.txt')) return `记录${moduleName(filePath)}相关的说明、约定或辅助资料。`;
  if (p.endsWith('.json') || p.endsWith('.ioc') || p.endsWith('.mxproject')) return `保存${moduleName(filePath)}相关的工具配置或工程配置。`;
  return `实现${moduleName(filePath)}模块中的固件逻辑。`;
}

function tagsFor(filePath, nodeType) {
  const p = normalizePath(filePath).toLowerCase();
  const tags = new Set();
  if (nodeType === 'config') tags.add('configuration');
  if (nodeType === 'document') tags.add('documentation');
  if (nodeType === 'file') tags.add('firmware');
  if (p.endsWith('.h')) tags.add('interface');
  if (p.endsWith('.c')) tags.add('implementation');
  if (p.includes('/motion/')) tags.add('motion');
  if (p.includes('/ai/')) tags.add('ai-inference');
  if (p.includes('lt_screen') || p.includes('lt168b') || p.includes('/display/')) tags.add('screen');
  if (p.includes('/cloud/') || p.includes('onenet') || p.includes('mqtt')) tags.add('cloud');
  if (p.includes('/ota/')) tags.add('ota');
  if (p.includes('/rule action/')) tags.add('rule-engine');
  if (p.includes('/core/')) tags.add('stm32-core');
  if (p.includes('/system/')) tags.add('system-support');
  if (p.includes('/bsp/')) tags.add('bsp-driver');
  if (tags.size < 3) tags.add('embedded');
  if (tags.size < 3) tags.add(topDir(filePath).toLowerCase().replace(/[^a-z0-9]+/g, '-'));
  return Array.from(tags).filter(Boolean).slice(0, 5);
}

function nodeTypeFor(file) {
  if (file.fileCategory === 'config') return 'config';
  if (file.fileCategory === 'docs') return 'document';
  return 'file';
}

function nodeIdForFile(file) {
  const type = nodeTypeFor(file);
  const prefix = type === 'config' ? 'config' : type === 'document' ? 'document' : 'file';
  return `${prefix}:${normalizePath(file.path)}`;
}

function complexity(lines) {
  if (lines > 220) return 'complex';
  if (lines > 70) return 'moderate';
  return 'simple';
}

function functionSummary(fn, filePath) {
  return `在${moduleName(filePath)}模块中实现 ${fn.name}，用于支撑该模块的核心状态处理、数据转换或对外接口。`;
}

function functionTags(fn, filePath) {
  const tags = new Set(['function']);
  const n = fn.name.toLowerCase();
  if (n.includes('init')) tags.add('initialization');
  if (n.includes('run') || n.includes('task')) tags.add('task-loop');
  if (n.includes('handle') || n.includes('process')) tags.add('event-handler');
  if (n.includes('request') || n.includes('queue') || n.includes('take') || n.includes('peek')) tags.add('state-machine');
  if (n.includes('send') || n.includes('report') || n.includes('publish')) tags.add('communication');
  if (n.includes('reset') || n.includes('clear')) tags.add('state-reset');
  for (const tag of tagsFor(filePath, 'file')) tags.add(tag);
  return Array.from(tags).slice(0, 5);
}

function isSignificantFunction(fn, result) {
  const len = Math.max(0, (fn.endLine || 0) - (fn.startLine || 0) + 1);
  const exported = new Set((result.exports || []).map(e => e.name));
  return len >= 10 || exported.has(fn.name);
}

function makeFileNode(file, result) {
  const type = nodeTypeFor(file);
  const lines = result?.nonEmptyLines || file.sizeLines || 0;
  return {
    id: nodeIdForFile(file),
    type,
    name: baseName(file.path),
    filePath: normalizePath(file.path),
    summary: purpose(file.path),
    tags: tagsFor(file.path, type),
    complexity: complexity(lines)
  };
}

function makeFunctionNode(fn, filePath) {
  const len = Math.max(0, (fn.endLine || 0) - (fn.startLine || 0) + 1);
  return {
    id: `function:${normalizePath(filePath)}:${fn.name}`,
    type: 'function',
    name: fn.name,
    filePath: normalizePath(filePath),
    lineRange: [fn.startLine || 1, fn.endLine || fn.startLine || 1],
    summary: functionSummary(fn, filePath),
    tags: functionTags(fn, filePath),
    complexity: complexity(len)
  };
}

for (const batch of batches) {
  const inputPath = path.join(tmpDir, `ua-file-analyzer-input-${batch.batchIndex}.json`);
  const extractPath = path.join(tmpDir, `ua-file-extract-results-${batch.batchIndex}.json`);
  fs.writeFileSync(inputPath, JSON.stringify({
    projectRoot,
    batchFiles: batch.files,
    batchImportData: batch.batchImportData
  }, null, 2));

  const res = cp.spawnSync('node', [
    path.join(skillDir, 'extract-structure.mjs'),
    inputPath,
    extractPath
  ], { cwd: projectRoot, encoding: 'utf8' });
  if (res.status !== 0) {
    process.stderr.write(res.stderr || res.stdout || `extract failed for batch ${batch.batchIndex}`);
    process.exit(res.status || 1);
  }

  const extracted = JSON.parse(fs.readFileSync(extractPath, 'utf8'));
  const resultByPath = new Map((extracted.results || []).map(r => [normalizePath(r.path), r]));
  const nodes = [];
  const edges = [];
  const functionIds = new Set();

  for (const file of batch.files) {
    const filePath = normalizePath(file.path);
    const result = resultByPath.get(filePath);
    const fileNode = makeFileNode(file, result);
    nodes.push(fileNode);

    for (const target of batch.batchImportData[file.path] || []) {
      const targetPath = normalizePath(target);
      if (targetPath !== filePath) {
        edges.push({
          source: fileNode.id,
          target: `file:${targetPath}`,
          type: 'imports',
          direction: 'forward',
          weight: 0.7
        });
      }
    }

    for (const fn of result?.functions || []) {
      if (!isSignificantFunction(fn, result)) continue;
      const fnNode = makeFunctionNode(fn, filePath);
      nodes.push(fnNode);
      functionIds.add(fnNode.id);
      edges.push({
        source: fileNode.id,
        target: fnNode.id,
        type: 'contains',
        direction: 'forward',
        weight: 1.0
      });
    }

    const exported = new Set((result?.exports || []).map(e => e.name));
    for (const exportName of exported) {
      const id = `function:${filePath}:${exportName}`;
      if (functionIds.has(id)) {
        edges.push({
          source: fileNode.id,
          target: id,
          type: 'exports',
          direction: 'forward',
          weight: 0.8
        });
      }
    }
  }

  const ownIds = new Set(nodes.map(n => n.id));
  const cleanEdges = [];
  const seenEdges = new Set();
  for (const edge of edges) {
    if (edge.source === edge.target) continue;
    const key = `${edge.source}|${edge.target}|${edge.type}`;
    if (seenEdges.has(key)) continue;
    seenEdges.add(key);
    cleanEdges.push(edge);
  }

  const outPath = path.join(intermediateDir, `batch-${batch.batchIndex}.json`);
  fs.writeFileSync(outPath, JSON.stringify({ nodes, edges: cleanEdges }, null, 2));
  console.log(`batch ${batch.batchIndex}: ${nodes.length} nodes, ${cleanEdges.length} edges`);
}
