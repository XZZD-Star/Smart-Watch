const fs = require('fs');
const path = require('path');

const root = process.argv[2];
const uaDir = path.join(root, '.understand-anything');
const intermediate = path.join(uaDir, 'intermediate');
const graph = JSON.parse(fs.readFileSync(path.join(intermediate, 'assembled-graph.json'), 'utf8'));
const scan = JSON.parse(fs.readFileSync(path.join(intermediate, 'scan-result.json'), 'utf8'));

fs.writeFileSync(path.join(uaDir, 'knowledge-graph.json'), JSON.stringify(graph, null, 2) + '\n', 'utf8');
fs.writeFileSync(path.join(intermediate, 'fingerprint-input.json'), JSON.stringify({
  projectRoot: root,
  sourceFilePaths: scan.files.map(file => file.path),
  gitCommitHash: graph.project.gitCommitHash
}, null, 2) + '\n', 'utf8');
