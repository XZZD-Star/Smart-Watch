const fs = require('fs');

try {
  const [inputPath, outputPath] = process.argv.slice(2);
  const input = JSON.parse(fs.readFileSync(inputPath, 'utf8'));
  const { nodes, edges, layers = [] } = input;
  const byId = new Map(nodes.map((node) => [node.id, node]));
  const fanIn = new Map(nodes.map((node) => [node.id, 0]));
  const fanOut = new Map(nodes.map((node) => [node.id, 0]));
  for (const edge of edges) {
    if (byId.has(edge.source)) fanOut.set(edge.source, fanOut.get(edge.source) + 1);
    if (byId.has(edge.target)) fanIn.set(edge.target, fanIn.get(edge.target) + 1);
  }
  const ranked = (counts, key) => nodes.map((node) => ({ id: node.id, [key]: counts.get(node.id), name: node.name }))
    .sort((a, b) => b[key] - a[key] || a.id.localeCompare(b.id)).slice(0, 20);
  const fanInRanking = ranked(fanIn, 'fanIn');
  const fanOutRanking = ranked(fanOut, 'fanOut');
  const outValues = [...fanOut.values()].sort((a, b) => a - b);
  const inValues = [...fanIn.values()].sort((a, b) => a - b);
  const topOutThreshold = outValues[Math.max(0, Math.floor(outValues.length * 0.9))] || 0;
  const lowInThreshold = inValues[Math.max(0, Math.floor(inValues.length * 0.25))] || 0;
  const entryNames = new Set(['index.ts','index.js','main.ts','main.js','app.ts','app.js','server.ts','server.js','mod.rs','main.go','main.py','main.rs','manage.py','app.py','wsgi.py','asgi.py','run.py','__main__.py','application.java','program.cs','config.ru','index.php','app.swift','application.kt','main.cpp','main.c']);
  const entryPointCandidates = nodes.map((node) => {
    const path = (node.filePath || node.name || '').replaceAll('\\', '/');
    const name = (node.name || path.split('/').pop() || '').toLowerCase();
    const depth = path.split('/').filter(Boolean).length;
    let score = 0;
    if (node.type === 'file') {
      if (entryNames.has(name)) score += 3;
      if (depth <= 2) score += 1;
      if (fanOut.get(node.id) >= topOutThreshold) score += 1;
      if (fanIn.get(node.id) <= lowInThreshold) score += 1;
    } else if (node.type === 'document') {
      if (path.toLowerCase() === 'readme.md') score += 5;
      else if (depth === 1 && name.endsWith('.md')) score += 2;
    }
    return { id: node.id, score, name: node.name, summary: node.summary };
  }).filter((item) => item.score > 0).sort((a, b) => b.score - a.score || a.id.localeCompare(b.id)).slice(0, 5);
  const start = entryPointCandidates.find((candidate) => byId.get(candidate.id)?.type === 'file');
  const traversal = { startNode: start?.id || null, order: [], depthMap: {}, byDepth: {} };
  if (start) {
    const allowedEdges = edges.filter((edge) => edge.type === 'imports' || edge.type === 'calls');
    const adjacency = new Map();
    for (const edge of allowedEdges) {
      if (byId.has(edge.source) && byId.has(edge.target)) {
        if (!adjacency.has(edge.source)) adjacency.set(edge.source, []);
        adjacency.get(edge.source).push(edge.target);
      }
    }
    const queue = [start.id];
    traversal.depthMap[start.id] = 0;
    for (let i = 0; i < queue.length; i++) {
      const id = queue[i];
      traversal.order.push(id);
      const depth = traversal.depthMap[id];
      if (!traversal.byDepth[depth]) traversal.byDepth[depth] = [];
      traversal.byDepth[depth].push(id);
      for (const next of adjacency.get(id) || []) {
        if (traversal.depthMap[next] === undefined) {
          traversal.depthMap[next] = depth + 1;
          queue.push(next);
        }
      }
    }
  }
  const mapNonCode = (types) => nodes.filter((node) => types.includes(node.type)).map(({ id, name, type, summary }) => ({ id, name, type, summary }));
  const nonCodeFiles = {
    documentation: mapNonCode(['document']),
    infrastructure: mapNonCode(['service', 'pipeline', 'resource']),
    data: mapNonCode(['table', 'schema', 'endpoint']),
    config: mapNonCode(['config'])
  };
  const directed = new Set(edges.filter((edge) => (edge.type === 'imports' || edge.type === 'calls') && byId.has(edge.source) && byId.has(edge.target)).map((edge) => `${edge.source}\u0000${edge.target}`));
  const clusters = [];
  const usedPairs = new Set();
  for (const edge of edges) {
    const pair = [edge.source, edge.target].sort();
    const pairKey = pair.join('\u0000');
    if (usedPairs.has(pairKey) || !directed.has(`${edge.target}\u0000${edge.source}`)) continue;
    usedPairs.add(pairKey);
    const cluster = new Set(pair);
    let expanded = true;
    while (expanded && cluster.size < 5) {
      expanded = false;
      for (const node of nodes) {
        if (cluster.has(node.id)) continue;
        let links = 0;
        for (const member of cluster) {
          if (directed.has(`${node.id}\u0000${member}`) || directed.has(`${member}\u0000${node.id}`)) links++;
        }
        if (links >= 2) { cluster.add(node.id); expanded = true; if (cluster.size >= 5) break; }
      }
    }
    const clusterNodes = [...cluster];
    const edgeCount = edges.filter((e) => cluster.has(e.source) && cluster.has(e.target)).length;
    clusters.push({ nodes: clusterNodes, edgeCount });
  }
  clusters.sort((a, b) => b.edgeCount - a.edgeCount || b.nodes.length - a.nodes.length);
  const nodeSummaryIndex = Object.fromEntries(nodes.map(({ id, name, type, summary }) => [id, { name, type, summary }]));
  const result = {
    scriptCompleted: true,
    entryPointCandidates,
    fanInRanking,
    fanOutRanking,
    bfsTraversal: traversal,
    nonCodeFiles,
    clusters: clusters.slice(0, 10),
    layers: { count: layers.length, list: layers.map(({ id, name, description }) => ({ id, name, description })) },
    nodeSummaryIndex,
    totalNodes: nodes.length,
    totalEdges: edges.length
  };
  fs.writeFileSync(outputPath, JSON.stringify(result, null, 2), 'utf8');
} catch (error) {
  console.error(error.stack || error.message);
  process.exit(1);
}
