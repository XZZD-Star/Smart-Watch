const fs = require('fs');

const inputPath = process.argv[2];
const outputPath = process.argv[3];
const levelTypes = new Set(['file', 'config', 'document', 'service', 'pipeline', 'table', 'schema', 'resource', 'endpoint']);

try {
  const graph = JSON.parse(fs.readFileSync(inputPath, 'utf8'));
  const fileNodes = graph.nodes.filter((node) => levelTypes.has(node.type));
  const ids = new Set(fileNodes.map((node) => node.id));
  const allEdges = graph.edges.filter((edge) => ids.has(edge.source) && ids.has(edge.target));
  const importEdges = allEdges.filter((edge) => edge.type === 'imports');
  fs.writeFileSync(outputPath, JSON.stringify({ fileNodes, importEdges, allEdges }, null, 2), 'utf8');
} catch (error) {
  console.error(error.stack || error.message);
  process.exit(1);
}
