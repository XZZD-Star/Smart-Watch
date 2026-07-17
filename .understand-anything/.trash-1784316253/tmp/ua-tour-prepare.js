const fs = require('fs');

try {
  const [graphPath, layersPath, outputPath] = process.argv.slice(2);
  const graph = JSON.parse(fs.readFileSync(graphPath, 'utf8'));
  const layers = JSON.parse(fs.readFileSync(layersPath, 'utf8'));
  const allowed = new Set(['file', 'config', 'document', 'service', 'pipeline', 'table', 'schema', 'resource', 'endpoint']);
  const nodes = graph.nodes.filter((node) => allowed.has(node.type));
  const edges = graph.edges;
  const cleanLayers = layers.map(({ id, name, description }) => ({ id, name, description }));
  fs.writeFileSync(outputPath, JSON.stringify({ nodes, edges, layers: cleanLayers }, null, 2), 'utf8');
} catch (error) {
  console.error(error.stack || error.message);
  process.exit(1);
}
