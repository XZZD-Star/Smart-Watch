import fs from 'node:fs';
import path from 'node:path';

const root = 'D:/Qian Start/Smart Watch/H743';
const tmp = path.join(root, '.understand-anything/tmp');
const outDir = path.join(root, '.understand-anything/intermediate');
const summaries = {
  'App/motion/motion_app_events.c':'维护动作识别结果、训练页刷新和云端上报请求的轻量事件状态，为任务与 UI/云连接之间提供无阻塞状态交接。',
  'App/motion/motion_app_events.h':'声明动作识别事件队列、训练页刷新和云端待上报状态的公共接口。',
  'App/motion/motion_input.c':'接收启动、停止、清除和中断触发请求，并协调姿态传感器流水线及窗口测试的输入状态。',
  'App/motion/motion_input.h':'定义运动识别任务的外部控制请求接口。',
  'App/motion/motion_mode.h':'定义运动识别、采集、校准与测试等运行模式枚举及相关模式约束。',
  'App/motion/motion_sensor_pipeline.c':'解析上下肢姿态数据包，完成时间戳与序号校验、双传感器帧融合、校准统计及 ISR 到任务的数据交接。',
  'App/motion/motion_sensor_pipeline.h':'声明姿态数据接收、融合帧提取、校准控制和流水线复位接口。',
  'App/motion/motion_task.c':'实现运动识别主任务，驱动融合帧处理、AI 推理、跌倒判定、训练结果事件和多种 UART/CSV 调试输出。',
  'App/motion/motion_task.h':'声明运动识别 FreeRTOS 任务入口。',
  'App/motion/motion_window_test.c':'管理离线模型窗口测试请求，执行内置测试数据推理并输出验证结果。',
  'App/motion/motion_window_test.h':'声明模型窗口测试的初始化、复位、请求和执行接口。',
  'Common/crc16.c':'提供流式 CRC16 初始化、增量更新和整块数据计算，用于 OTA 元数据完整性校验。',
  'Common/crc16.h':'声明通用 CRC16 校验接口及初始值常量。',
  'Common/md5.c':'实现 MD5 分组变换、流式摘要计算、十六进制编码与字符串合法性检查。',
  'Common/md5.h':'定义 MD5 上下文并声明摘要计算和十六进制辅助接口。',
  'Common/ota_device_info.c':'在冗余 Flash 槽位中读取、校验和保存设备当前运行版本，利用序号选择最新有效记录。',
  'Common/ota_device_info.h':'定义设备 OTA 版本持久化记录及加载、查询和保存接口。',
  'Common/ota_info.c':'持久化并校验待升级固件和模拟升级信息，结合 CRC16 与 MD5 保证元数据有效性。',
  'Common/ota_info.h':'定义 OTA 待升级记录结构及加载、保存和失效接口。',
  'App/ota/ota_config.h':'集中定义 OTA 服务地址、请求路径、下载分块和 Flash 区域等编译期配置。',
  'App/ota/ota_http.c':'基于网络连接实现受限 HTTP 请求与响应解析，以回调方式交付响应体并处理 Content-Length 等头字段。',
  'App/ota/ota_http.h':'定义 OTA HTTP 请求参数、响应信息和流式响应体回调接口。',
  'App/ota/ota_service.c':'实现 OTA 完整业务流程，包括任务查询、JSON 解析、分段固件下载、Flash 写入、MD5 校验、升级信息保存和版本上报。',
  'App/ota/ota_service.h':'声明 OTA 初始化、周期检查、模拟升级和版本上报接口。',
  'BSP/cloud/MqttKit.c':'实现 MQTT 报文缓冲区管理及 CONNECT、SUBSCRIBE、PUBLISH、PING 等报文的编码与解码。',
  'BSP/cloud/MqttKit.h':'定义 MQTT 报文类型、缓冲区结构和协议编解码接口。',
  'BSP/cloud/onenet.c':'实现 OneNET MQTT 会话、主题订阅、属性上报与属性设置下行处理，并联动训练计划、门状态、跌倒告警和运动测试。',
  'BSP/cloud/onenet.h':'声明 OneNET 设备连接、订阅发布、下行处理及会话状态查询接口。'
};

const action = n => {
  const rules = [['Init','初始化'],['Reset','复位'],['Parse','解析'],['Load','加载'],['Save','保存'],['Read','读取'],['Write','写入'],['Post','上报'],['Publish','发布'],['Subscribe','订阅'],['Request','请求'],['Take','提取'],['Peek','查看'],['Clear','清除'],['Check','检查'],['Update','更新'],['Handle','处理'],['Apply','应用'],['Download','下载'],['Send','发送'],['Encode','编码'],['Decode','解码'],['Calc','计算'],['Is','判断'],['Has','判断'],['Get','获取'],['Set','设置'],['Run','执行'],['Process','处理'],['Copy','复制'],['Make','生成'],['Sync','同步'],['Query','查询'],['Packet','构造'],['UnPacket','解包']];
  for (const [k,v] of rules) if (n.includes(k)) return `${v} ${n} 对应的模块状态或数据流程。`;
  return `实现 ${n} 对应的内部处理流程。`;
};
const fnTags = n => {
  const t=['函数','嵌入式'];
  if (/Parse|Decode|UnPacket|ReadLength/.test(n)) t.push('协议解析');
  else if (/Packet|Encode|Write|Publish|Post|Send/.test(n)) t.push('数据输出');
  else if (/CRC|MD5|Valid|Check/.test(n)) t.push('数据校验');
  else if (/Calibration/.test(n)) t.push('传感器校准');
  else if (/Reset|Clear|Init/.test(n)) t.push('状态管理');
  else if (/Is|Has|Get|Peek|Take/.test(n)) t.push('状态查询');
  else t.push('业务逻辑');
  return t;
};
const fileTags = p => {
  if (p.includes('/motion/')) return ['运动识别','传感器融合',p.endsWith('.h')?'接口定义':'任务处理'];
  if (p.includes('/ota/')) return ['ota','固件升级',p.endsWith('.h')?'配置接口':'网络服务'];
  if (p.includes('/cloud/')) return ['mqtt','onenet',p.endsWith('.h')?'协议接口':'云端通信'];
  if (p.includes('md5')) return ['md5','数据校验','工具函数'];
  if (p.includes('crc16')) return ['crc16','数据校验','工具函数'];
  return ['ota','持久化','完整性校验'];
};
const complexity = n => n > 200 ? 'complex' : n >= 50 ? 'moderate' : 'simple';

for (const bi of [1,2,3,4]) {
  const input=JSON.parse(fs.readFileSync(path.join(tmp,`ua-file-analyzer-input-${bi}.json`),'utf8'));
  const ext=JSON.parse(fs.readFileSync(path.join(tmp,`ua-file-extract-results-${bi}.json`),'utf8'));
  const nodes=[], edges=[];
  for (const f of ext.results) {
    const fid=`file:${f.path}`;
    nodes.push({id:fid,type:'file',name:path.basename(f.path),filePath:f.path,summary:summaries[f.path],tags:fileTags(f.path),complexity:complexity(f.nonEmptyLines),languageNotes:f.path.endsWith('.c')?'采用 STM32 C 模块化接口，使用文件内 static 状态封装实现细节。':undefined});
    const exported=new Set((f.exports||[]).map(x=>x.name));
    for (const fn of f.functions||[]) {
      const lines=fn.endLine-fn.startLine+1;
      if (lines<10 && !exported.has(fn.name)) continue;
      const id=`function:${f.path}:${fn.name}`;
      nodes.push({id,type:'function',name:fn.name,filePath:f.path,lineRange:[fn.startLine,fn.endLine],summary:action(fn.name),tags:fnTags(fn.name),complexity:complexity(lines)});
      edges.push({source:fid,target:id,type:'contains',direction:'forward',weight:1.0});
      if (exported.has(fn.name)) edges.push({source:fid,target:id,type:'exports',direction:'forward',weight:0.8});
    }
    for (const target of input.batchImportData[f.path]||[]) edges.push({source:fid,target:`file:${target}`,type:'imports',direction:'forward',weight:0.7});
  }
  const filePaths=[...new Set(ext.results.map(x=>x.path))].sort();
  const parts=Math.ceil(Math.max(nodes.length/60,edges.length/120));
  if (parts===1) fs.writeFileSync(path.join(outDir,`batch-${bi}.json`),JSON.stringify({nodes,edges},null,2)+'\n');
  else {
    // 将旧的单文件产物置为空片段，避免重建前的损坏内容参与后续合并。
    fs.writeFileSync(path.join(outDir,`batch-${bi}.json`),JSON.stringify({nodes:[],edges:[]},null,2)+'\n');
    const chunk=Math.ceil(filePaths.length/parts);
    for(let k=0;k<parts;k++){
      const paths=new Set(filePaths.slice(k*chunk,(k+1)*chunk));
      const pn=nodes.filter(n=>paths.has(n.filePath)); const ids=new Set(pn.map(n=>n.id));
      const pe=edges.filter(e=>ids.has(e.source));
      fs.writeFileSync(path.join(outDir,`batch-${bi}-part-${k+1}.json`),JSON.stringify({nodes:pn,edges:pe},null,2)+'\n');
    }
  }
}
