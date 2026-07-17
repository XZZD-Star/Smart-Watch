const fs = require('fs');
const path = require('path');

const inputPath = process.argv[2];
const outputPath = process.argv[3];

function normalize(value) { return (value || '').replace(/\\/g, '/'); }
function commonDirectoryPrefix(paths) {
  if (!paths.length) return [];
  const parts = paths.map((p) => normalize(p).split('/').slice(0, -1));
  const result = [];
  for (let i = 0; i < Math.min(...parts.map((p) => p.length)); i += 1) {
    if (parts.every((p) => p[i] === parts[0][i])) result.push(parts[0][i]); else break;
  }
  return result;
}
function groupName(filePath, prefix) {
  const parts = normalize(filePath).split('/');
  const remaining = parts.slice(prefix.length);
  return remaining.length > 1 ? remaining[0] : 'root';
}
function patternFor(group, nodes) {
  const patterns = {
    api: ['routes','api','controllers','endpoints','handlers','serializers','controller','routers','blueprints'],
    service: ['services','core','lib','domain','logic','internal','signals','composables','mailers','jobs','channels'],
    data: ['models','db','data','persistence','repository','entities','entity','migrations','sql','database','schema'],
    ui: ['components','views','pages','ui','layouts','screens'], middleware: ['middleware','plugins','interceptors','guards'],
    utility: ['utils','helpers','common','shared','tools','pkg','templatetags'], config: ['config','constants','env','settings','management','commands'],
    test: ['__tests__','test','tests','spec','specs'], types: ['types','interfaces','schemas','contracts','dtos','dto','request','response'],
    hooks: ['hooks'], state: ['store','state','reducers','actions','slices'], assets: ['assets','static','public'], entry: ['cmd','bin'],
    documentation: ['docs','documentation','wiki'], infrastructure: ['deploy','deployment','infra','infrastructure','k8s','kubernetes','helm','charts','terraform','tf','docker'],
    'ci-cd': ['.github','.gitlab','.circleci']
  };
  const lower = group.toLowerCase();
  for (const [label, names] of Object.entries(patterns)) if (names.includes(lower)) return label;
  const names = nodes.map((n) => normalize(n.filePath).toLowerCase());
  if (names.some((n) => /(^|\/)(dockerfile|docker-compose\.|jenkinsfile)|\.tf(vars)?$/.test(n))) return 'infrastructure';
  if (names.every((n) => /\.(md|rst)$/.test(n))) return 'documentation';
  if (names.every((n) => /(^|\/)([^/]+\.(test|spec)\.|test_[^/]+\.py|[^/]+_test\.go)/.test(n))) return 'test';
  return null;
}

try {
  const input = JSON.parse(fs.readFileSync(inputPath, 'utf8'));
  const nodes = input.fileNodes;
  const byId = new Map(nodes.map((n) => [n.id, n]));
  const prefix = commonDirectoryPrefix(nodes.map((n) => n.filePath));
  const directoryGroups = {};
  const nodeGroup = new Map();
  for (const node of nodes) {
    const group = groupName(node.filePath, prefix);
    (directoryGroups[group] ||= []).push(node.id);
    nodeGroup.set(node.id, group);
  }
  const nodeTypeGroups = {};
  for (const node of nodes) (nodeTypeGroups[node.type] ||= []).push(node.id);
  const fileFanIn = Object.fromEntries(nodes.map((n) => [n.id, 0]));
  const fileFanOut = Object.fromEntries(nodes.map((n) => [n.id, 0]));
  const inter = new Map();
  const involved = Object.fromEntries(Object.keys(directoryGroups).map((g) => [g, 0]));
  const internal = Object.fromEntries(Object.keys(directoryGroups).map((g) => [g, 0]));
  for (const edge of input.importEdges) {
    fileFanOut[edge.source] += 1; fileFanIn[edge.target] += 1;
    const from = nodeGroup.get(edge.source), to = nodeGroup.get(edge.target);
    if (from === to) { internal[from] += 1; involved[from] += 1; }
    else { involved[from] += 1; involved[to] += 1; inter.set(`${from}\0${to}`, (inter.get(`${from}\0${to}`) || 0) + 1); }
  }
  const interGroupImports = [...inter.entries()].map(([key, count]) => { const [from, to] = key.split('\0'); return { from, to, count }; });
  const intraGroupDensity = Object.fromEntries(Object.keys(directoryGroups).map((g) => [g, { internalEdges: internal[g], totalEdges: involved[g], density: involved[g] ? internal[g] / involved[g] : 0 }]));
  const cross = new Map();
  const nonCodeConnections = [];
  for (const edge of input.allEdges) {
    const fromType = byId.get(edge.source).type, toType = byId.get(edge.target).type;
    if (fromType !== toType) {
      const key = `${fromType}\0${toType}\0${edge.type}`;
      cross.set(key, (cross.get(key) || 0) + 1);
      if (fromType !== 'file' || toType !== 'file') nonCodeConnections.push(edge);
    }
  }
  const crossCategoryEdges = [...cross.entries()].map(([key, count]) => { const [fromType,toType,edgeType] = key.split('\0'); return {fromType,toType,edgeType,count}; });
  const patternMatches = Object.fromEntries(Object.entries(directoryGroups).map(([g, ids]) => [g, patternFor(g, ids.map((id) => byId.get(id)))]));
  const paths = nodes.map((n) => normalize(n.filePath));
  const infraFiles = paths.filter((p) => /(^|\/)(dockerfile[^/]*|docker-compose\.[^/]+|jenkinsfile)$|\.tf(vars)?$|(^|\/)(k8s|kubernetes|helm|charts|\.github\/workflows)(\/|$)/i.test(p));
  const deploymentTopology = { hasDockerfile: infraFiles.some((p) => /dockerfile/i.test(p)), hasCompose: infraFiles.some((p) => /docker-compose/i.test(p)), hasK8s: infraFiles.some((p) => /\/(k8s|kubernetes|helm|charts)\//i.test(`/${p}/`)), hasTerraform: infraFiles.some((p) => /\.tf(vars)?$/i.test(p)), hasCI: infraFiles.some((p) => /\.github\/workflows|jenkinsfile|gitlab-ci/i.test(p)), infraFiles };
  const dataPipeline = { schemaFiles: paths.filter((p) => /\.(sql|graphql|gql|proto|prisma)$/i.test(p)), migrationFiles: paths.filter((p) => /(^|\/)migrations?\//i.test(p)), dataModelFiles: paths.filter((p) => /(^|\/)(models?|entities|data)\//i.test(p)), apiHandlerFiles: paths.filter((p) => /(^|\/)(routes?|controllers?|handlers?|api)\//i.test(p)) };
  const docs = nodes.filter((n) => n.type === 'document' || /\.(md|rst)$/i.test(n.filePath));
  const groupsWithDocsSet = new Set(docs.map((n) => nodeGroup.get(n.id)));
  const groups = Object.keys(directoryGroups);
  const docCoverage = { groupsWithDocs: groupsWithDocsSet.size, totalGroups: groups.length, coverageRatio: groups.length ? groupsWithDocsSet.size / groups.length : 0, undocumentedGroups: groups.filter((g) => !groupsWithDocsSet.has(g)) };
  const pairs = new Set(interGroupImports.map((x) => [x.from,x.to].sort().join('\0')));
  const dependencyDirection = [...pairs].map((pair) => { const [a,b] = pair.split('\0'); const ab = inter.get(`${a}\0${b}`)||0, ba=inter.get(`${b}\0${a}`)||0; return ab >= ba ? {dependent:a,dependsOn:b,count:ab,reverseCount:ba}:{dependent:b,dependsOn:a,count:ba,reverseCount:ab}; });
  const result = { scriptCompleted:true, commonPrefix:prefix.join('/'), directoryGroups, nodeTypeGroups, crossCategoryEdges, nonCodeConnections, interGroupImports, intraGroupDensity, patternMatches, deploymentTopology, dataPipeline, docCoverage, dependencyDirection, fileStats:{totalFileNodes:nodes.length,filesPerGroup:Object.fromEntries(Object.entries(directoryGroups).map(([g,ids])=>[g,ids.length])),nodeTypeCounts:Object.fromEntries(Object.entries(nodeTypeGroups).map(([t,ids])=>[t,ids.length]))}, fileFanIn, fileFanOut };
  fs.writeFileSync(outputPath, JSON.stringify(result, null, 2), 'utf8');
} catch (error) { console.error(error.stack || error.message); process.exit(1); }
