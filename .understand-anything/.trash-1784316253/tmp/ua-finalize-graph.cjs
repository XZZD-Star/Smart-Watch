const fs = require('fs');
const path = require('path');
const cp = require('child_process');

const root = 'D:/Qian Start/Smart Watch/H743';
const uaDir = path.join(root, '.understand-anything');
const intermediate = path.join(uaDir, 'intermediate');
const scan = JSON.parse(fs.readFileSync(path.join(intermediate, 'scan-result.json'), 'utf8'));
const assembled = JSON.parse(fs.readFileSync(path.join(intermediate, 'assembled-graph.json'), 'utf8'));
const layers = JSON.parse(fs.readFileSync(path.join(intermediate, 'layers.json'), 'utf8'));
const tour = JSON.parse(fs.readFileSync(path.join(intermediate, 'tour.json'), 'utf8'));

const commit = cp.spawnSync('git', ['rev-parse', 'HEAD'], { cwd: root, encoding: 'utf8' }).stdout.trim();

const graph = {
  version: '1.0.0',
  project: {
    name: scan.name,
    languages: scan.languages,
    frameworks: scan.frameworks,
    description: scan.description,
    analyzedAt: new Date().toISOString(),
    gitCommitHash: commit
  },
  nodes: assembled.nodes,
  edges: assembled.edges,
  layers,
  tour
};

function validate(g) {
  const issues = [];
  const warnings = [];
  const nodeIds = new Set();
  const seen = new Set();

  for (const [i, n] of g.nodes.entries()) {
    if (!n.id) issues.push(`Node[${i}] missing id`);
    if (!n.type) issues.push(`Node[${i}] ${n.id || '<missing>'} missing type`);
    if (!n.name) issues.push(`Node[${i}] ${n.id || '<missing>'} missing name`);
    if (!n.summary) issues.push(`Node[${i}] ${n.id || '<missing>'} missing summary`);
    if (!Array.isArray(n.tags) || n.tags.length === 0) issues.push(`Node[${i}] ${n.id || '<missing>'} missing tags`);
    if (n.id && seen.has(n.id)) issues.push(`Duplicate node ID ${n.id}`);
    if (n.id) {
      seen.add(n.id);
      nodeIds.add(n.id);
    }
  }

  for (const [i, e] of g.edges.entries()) {
    if (!nodeIds.has(e.source)) issues.push(`Edge[${i}] source ${e.source} not found`);
    if (!nodeIds.has(e.target)) issues.push(`Edge[${i}] target ${e.target} not found`);
  }

  const fileLevelTypes = new Set(['file', 'config', 'document', 'service', 'pipeline', 'table', 'schema', 'resource', 'endpoint']);
  const fileNodeIds = g.nodes.filter(n => fileLevelTypes.has(n.type)).map(n => n.id);
  const assigned = new Map();
  for (const layer of g.layers) {
    if (!layer.id || !layer.name || !layer.description || !Array.isArray(layer.nodeIds)) {
      issues.push(`Layer ${layer.id || '<missing>'} missing required fields`);
      continue;
    }
    for (const id of layer.nodeIds) {
      if (!nodeIds.has(id)) issues.push(`Layer ${layer.id} refs missing node ${id}`);
      if (assigned.has(id)) issues.push(`Node ${id} appears in multiple layers`);
      assigned.set(id, layer.id);
    }
  }
  for (const id of fileNodeIds) {
    if (!assigned.has(id)) issues.push(`File node ${id} not in any layer`);
  }

  for (const [i, step] of g.tour.entries()) {
    if (step.order !== i + 1) issues.push(`Tour step ${i} order is not sequential`);
    if (!step.title || !step.description || !Array.isArray(step.nodeIds) || step.nodeIds.length === 0) {
      issues.push(`Tour step ${i + 1} missing required fields`);
    }
    for (const id of step.nodeIds || []) {
      if (!nodeIds.has(id)) issues.push(`Tour step ${i + 1} refs missing node ${id}`);
    }
  }

  const withEdges = new Set();
  for (const e of g.edges) {
    withEdges.add(e.source);
    withEdges.add(e.target);
  }
  for (const n of g.nodes) {
    if (!withEdges.has(n.id)) warnings.push(`Node ${n.id} has no edges`);
  }

  const stats = {
    totalNodes: g.nodes.length,
    totalEdges: g.edges.length,
    totalLayers: g.layers.length,
    tourSteps: g.tour.length,
    nodeTypes: g.nodes.reduce((a, n) => ((a[n.type] = (a[n.type] || 0) + 1), a), {}),
    edgeTypes: g.edges.reduce((a, e) => ((a[e.type] = (a[e.type] || 0) + 1), a), {})
  };

  return { issues, warnings, stats };
}

const review = validate(graph);
fs.writeFileSync(path.join(intermediate, 'review.json'), JSON.stringify(review, null, 2));
fs.writeFileSync(path.join(intermediate, 'assembled-graph.json'), JSON.stringify(graph, null, 2));
fs.writeFileSync(path.join(uaDir, 'knowledge-graph.json'), JSON.stringify(graph, null, 2));

console.log(JSON.stringify(review.stats, null, 2));
if (review.issues.length) {
  console.error(`issues=${review.issues.length}`);
  for (const issue of review.issues.slice(0, 20)) console.error(issue);
  process.exit(1);
}
console.log(`warnings=${review.warnings.length}`);
